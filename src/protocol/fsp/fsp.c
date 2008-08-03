/* Internal FSP protocol implementation */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <fsplib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/protocol.h"
#include "protocol/fsp/fsp.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


struct option_info fsp_options[] = {
	INIT_OPT_TREE("protocol", N_("FSP"),
		"fsp", 0,
		N_("FSP specific options.")),

	INIT_OPT_BOOL("protocol.fsp", N_("Sort entries"),
		"sort", 0, 1,
		N_("Whether to sort entries in directory listings.")),

	NULL_OPTION_INFO,
};

struct module fsp_protocol_module = struct_module(
	/* name: */		N_("FSP"),
	/* options: */		fsp_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


/* Because functions of fsplib block waiting for a response from the
 * server, and ELinks wants non-blocking operations so that other
 * connections and the user interface keep working, this FSP protocol
 * module forks a child process for each FSP connection.  The child
 * process writes the results to two pipes, which the main ELinks
 * process then reads in a non-blocking fashion.  The child process
 * gets these pipes as its stdout and stderr.
 *
 * - If an error occurs, the child process writes "text/x-error"
 *   without newline to stderr, and an error code and a newline to
 *   stdout.  The error code is either "S" followed by errno or "I"
 *   followed by enum connection_basic_state.  In particular, EPERM
 *   causes the parent process to prompt for username and password.
 *   (In this, fsplib differs from libsmbclient, which uses EACCES if
 *   authentication fails.)
 *
 * - If the resource is a regular file, the child process writes the
 *   estimated length of the file (in bytes) and a newline to stderr,
 *   and the contents of the file to stdout.
 *
 * - If the resource is a directory, the child process writes
 *   "text/html" without newline to stderr, and an HTML rendering
 *   of the directory listing to stdout.
 *
 * The exit code of the child process also indicates whether an error
 * occurred, but the parent process ignores it.  */

/* FSP synchronous connection management (child process):
 *
 * The child process generally does not bother to free the memory it
 * allocates.  When the process exits, the operating system will free
 * the memory anyway.  There is no point in changing this, because the
 * child process also inherits memory allocations from the parent
 * process, and it would be very cumbersome to free those.  */

/* FIXME: Although it is probably not so much an issue, check if writes to
 * stdout fails for directory listing like we do for file fetching. */

static void
fsp_error(struct connection_state error)
{
	if (is_system_error(error))
		printf("S%d\n", (int) error.syserr);
	else
		printf("I%d\n", (int) error.basic);
	fprintf(stderr, "text/x-error");
	/* In principle, this should perhaps call fsp_close_session to
	 * make the server accept any key from the next client process
	 * at this IP address.  That doesn't seem necessary though:
	 * fsplib uses various IPC schemes to synchronize the use of
	 * server-provided keys between client processes, so the next
	 * client process will probably be able to use the key saved
	 * by this one.  */
	exit(1);
}

static int
compare(const void *av, const void *bv)
{
	const FSP_RDENTRY *a = av, *b = bv;
	int res = ((b->type == FSP_RDTYPE_DIR) - (a->type == FSP_RDTYPE_DIR));

	if (res)
		return res;
	return strcmp(a->name, b->name);
}

static void
display_entry(const FSP_RDENTRY *fentry, const unsigned char dircolor[])
{
	struct string string;

	/* fentry->name is a fixed-size array and is followed by other
	 * members; thus, if the name reported by the server does not
	 * fit in the array, fsplib must either truncate or reject it.
	 * If fsplib truncates the name, it does not document whether
	 * fentry->namlen is the original length or the truncated
	 * length.  ELinks therefore ignores fentry->namlen and
	 * instead measures the length on its own.  */
	const size_t namelen = strlen(fentry->name);

	if (!init_string(&string)) return;
	add_format_to_string(&string, "%10d", fentry->size);
	add_to_string(&string, "\t<a href=\"");
	/* The result of encode_uri_string does not include '&' or '<'
	 * which could mess up the HTML.  */
	encode_uri_string(&string, fentry->name, namelen, 0);
	if (fentry->type == FSP_RDTYPE_DIR) {
		add_to_string(&string, "/\">");
		if (*dircolor) {
			add_to_string(&string, "<font color=\"");
			add_to_string(&string, dircolor);
			add_to_string(&string, "\"><b>");
		}
		add_html_to_string(&string, fentry->name, namelen);
		if (*dircolor) {
			add_to_string(&string, "</b></font>");
		}
	} else {
		add_to_string(&string, "\">");
		add_html_to_string(&string, fentry->name, namelen);
	}
	add_to_string(&string, "</a>");
	puts(string.source);
	done_string(&string);
}

static void
sort_and_display_entries(FSP_DIR *dir, const unsigned char dircolor[])
{
	/* fsp_readdir_native in fsplib 0.9 and earlier requires
	 * the third parameter to point to a non-null pointer
	 * even though it does not dereference that pointer
	 * and overwrites it with another one anyway.
	 * http://sourceforge.net/tracker/index.php?func=detail&aid=1875210&group_id=93841&atid=605738
	 * Work around the bug by using non-null &tmp.
	 * Nothing will actually read or write tmp.  */
	FSP_RDENTRY fentry, tmp, *table = NULL;
	FSP_RDENTRY *fresult = &tmp;
	int size = 0;
	int i;

	while (!fsp_readdir_native(dir, &fentry, &fresult)) {
		FSP_RDENTRY *new_table;

		if (!fresult) break;
		if (!strcmp(fentry.name, "."))
			continue;
		new_table = mem_realloc(table, (size + 1) * sizeof(*table));
		if (!new_table)
			continue;
		table = new_table;
		copy_struct(&table[size], &fentry);
		size++;
	}
	/* If size==0, then table==NULL.  According to ISO/IEC 9899:1999
	 * 7.20.5p1, the NULL must not be given to qsort.  */
	if (size > 0)
		qsort(table, size, sizeof(*table), compare);

	for (i = 0; i < size; i++) {
		display_entry(&table[i], dircolor);
	}
}

static void
fsp_directory(FSP_SESSION *ses, struct uri *uri)
{
	struct string buf;
	FSP_DIR *dir;
	unsigned char *data = get_uri_string(uri, URI_DATA);
	unsigned char dircolor[8] = "";

	if (!data)
		fsp_error(connection_state(S_OUT_OF_MEM));
	decode_uri(data);
	if (!is_in_state(init_directory_listing(&buf, uri), S_OK))
		fsp_error(connection_state(S_OUT_OF_MEM));

	dir = fsp_opendir(ses, data);
	if (!dir) fsp_error(connection_state_for_errno(errno));

	fprintf(stderr, "text/html");
	fclose(stderr);

	puts(buf.source);

	if (get_opt_bool("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				dircolor);
	}

	if (get_opt_bool("protocol.fsp.sort")) {
		sort_and_display_entries(dir, dircolor);
	} else {
		/* &tmp works around a bug in fsplib 0.9 or earlier.
		 * See sort_and_display_entries for details.  */
		FSP_RDENTRY fentry, tmp;
		FSP_RDENTRY *fresult = &tmp;

		while (!fsp_readdir_native(dir, &fentry, &fresult)) {
			if (!fresult) break;
			display_entry(&fentry, dircolor);
		}
	}
	fsp_closedir(dir);
	puts("</pre><hr/></body></html>");
	fsp_close_session(ses);
	exit(0);
}

#define READ_SIZE	4096

static void
do_fsp(struct connection *conn)
{
	FSP_SESSION *ses;
	struct stat sb;
	struct uri *uri = conn->uri;
	struct auth_entry *auth;
	unsigned char *host = get_uri_string(uri, URI_HOST);
	unsigned char *data = get_uri_string(uri, URI_DATA);
	unsigned short port = (unsigned short)get_uri_port(uri);
	unsigned char *password = NULL;

	decode_uri(data);
	if (uri->passwordlen) {
		password = get_uri_string(uri, URI_PASSWORD);
	} else {
		auth = find_auth(uri);
		if (auth) password = auth->password;
	}

	/* fsp_open_session may not set errno if getaddrinfo fails
	 * https://sourceforge.net/tracker/index.php?func=detail&aid=2036798&group_id=93841&atid=605738
	 * Try to detect this bug and use an ELinks-specific error
	 * code instead, so that we can display a message anyway.  */
	errno = 0;
	ses = fsp_open_session(host, port, password);
	if (!ses) {
		if (errno)
			fsp_error(connection_state_for_errno(errno));
		else
			fsp_error(connection_state(S_FSP_OPEN_SESSION_UNKN));
	}

	/* fsplib 0.8 ABI depends on _FILE_OFFSET_BITS
	 * https://sourceforge.net/tracker/index.php?func=detail&aid=1674729&group_id=93841&atid=605738
	 * If ELinks and fsplib are using different values of
	 * _FILE_OFFSET_BITS, then they get different definitions of
	 * struct stat, and the st_size stored by fsp_stat is
	 * typically not the same as the st_size read by ELinks.
	 * Fortunately, st_mode seems to have the same offset and size
	 * in both versions of struct stat.
	 *
	 * If all the bytes used by the 32-bit st_size are also used
	 * by the 64-bit st_size, then ELinks may be able to guess
	 * which ones they are, because the current version 2 of FSP
	 * supports only 32-bit file sizes in protocol packets.  Begin
	 * by filling struct stat with 0xAA so that it's easier to
	 * detect which bytes fsp_stat has left unchanged.  (Only
	 * sb.st_size really needs to be filled, but filling the rest
	 * too helps viewing the data with a debugger.)  */
	memset(&sb, 0xAA, sizeof(sb));
	if (fsp_stat(ses, data, &sb)) fsp_error(connection_state_for_errno(errno));

	if (S_ISDIR(sb.st_mode)) {
		fsp_directory(ses, uri);
	} else { /* regular file */
		char buf[READ_SIZE];
		FSP_FILE *file = fsp_fopen(ses, data, "r");
		int r;

		if (!file) {
			fsp_error(connection_state_for_errno(errno));
		}

#if SIZEOF_OFF_T >= 8
		if (sb.st_size < 0 || sb.st_size > 0xFFFFFFFF) {
			/* Probably a _FILE_OFFSET_BITS mismatch as
			 * described above.  Try to detect which half
			 * of st_size is the real size.  This may
			 * depend on the endianness of the processor
			 * and on the padding in struct stat.  */
			if ((sb.st_size & 0xFFFFFFFF00000000ULL) == 0xAAAAAAAA00000000ULL)
				sb.st_size = sb.st_size & 0xFFFFFFFF;
			else if ((sb.st_size & 0xFFFFFFFF) == 0xAAAAAAAA)
				sb.st_size = (sb.st_size >> 32) & 0xFFFFFFFF;
			else	/* Can't figure it out. */
				sb.st_size = 1;
		}
#endif

		/* Send filesize */
		fprintf(stderr, "%" OFF_PRINT_FORMAT "\n",
			(off_print_T) sb.st_size);
		fclose(stderr);

		while ((r = fsp_fread(buf, 1, READ_SIZE, file)) > 0) {
			int off = 0;

			while (r) {
				int w = safe_write(STDOUT_FILENO, buf + off, r);

				if (w == -1) goto out;
				off += w;
				r -= w;
			}
		}
out:
		fsp_fclose(file);
		fsp_close_session(ses);
		exit(0);
	}
}

#undef READ_SIZE



/* FSP asynchronous connection management (parent process): */

/* Kill the current connection and ask for a username/password for the next
 * try. */
static void
prompt_username_pw(struct connection *conn)
{
	add_auth_entry(conn->uri, "FSP", NULL, NULL, 0);
	abort_connection(conn, connection_state(S_OK));
}

static void
fsp_got_error(struct socket *socket, struct read_buffer *rb)
{
	int len = rb->length;
	struct connection *conn = socket->conn;
	struct connection_state error;

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	/* There should be free space in the buffer, because
	 * @alloc_read_buffer allocated several kibibytes, and the
	 * child process wrote only an integer and a newline to the
	 * pipe.  */
	assert(rb->freespace >= 1);
	if_assert_failed {
		abort_connection(conn, connection_state(S_INTERNAL));
		return;
	}
	rb->data[len] = '\0';
	switch (rb->data[0]) {
	case 'S':
		error = connection_state_for_errno(atoi(rb->data + 1));
		break;
	case 'I':
		error = connection_state(atoi(rb->data + 1));
		break;
	default:
		ERROR("malformed error code: %s", rb->data);
		error = connection_state(S_INTERNAL);
		break;
	}
	kill_buffer_data(rb, len);

	if (is_system_error(error) && error.syserr == EPERM)
		prompt_username_pw(conn);
	else
		abort_connection(conn, error);
}

static void
fsp_got_data(struct socket *socket, struct read_buffer *rb)
{
	int len = rb->length;
	struct connection *conn = socket->conn;

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (!len) {
		abort_connection(conn, connection_state(S_OK));
		return;
	}

	socket->state = SOCKET_END_ONCLOSE;
	conn->received += len;
	if (add_fragment(conn->cached, conn->from, rb->data, len) == 1)
		conn->tries = 0;
	conn->from += len;
	kill_buffer_data(rb, len);

	read_from_socket(socket, rb, connection_state(S_TRANS), fsp_got_data);
}

static void
fsp_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct read_buffer *buf;
	int error = 0;

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		/* Even though these are pipes rather than real
		 * sockets, call close_socket instead of close, to
		 * ensure that abort_connection won't try to close the
		 * file descriptors again.  (Could we skip the calls
		 * and assume abort_connection will do them?)  */
		close_socket(socket);
		close_socket(conn->data_socket);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	socket->state = SOCKET_END_ONCLOSE;

	if (rb->length > 0) {
		unsigned char *ctype = memacpy(rb->data, rb->length);

		if (ctype && *ctype) {
			if (!strcmp(ctype, "text/x-error")) {
				error = 1;
				mem_free(ctype);
			} else {
				if (ctype[0] >= '0' && ctype[0] <= '9') {
#ifdef HAVE_ATOLL
					conn->est_length = (off_t)atoll(ctype);
#else
					conn->est_length = (off_t)atol(ctype);
#endif
					mem_free(ctype);

					/* avoid read from socket error */
					if (!conn->est_length) {
						abort_connection(conn, connection_state(S_OK));
						return;
					}
				}
				else mem_free_set(&conn->cached->content_type, ctype);
			}
		} else {
			mem_free_if(ctype);
		}
	}

	buf = alloc_read_buffer(conn->data_socket);
	if (!buf) {
		close_socket(socket);
		close_socket(conn->data_socket);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (error) {
		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
		read_from_socket(conn->data_socket, buf,
				 connection_state(S_CONN), fsp_got_error);
	} else {
		read_from_socket(conn->data_socket, buf,
				 connection_state(S_CONN), fsp_got_data);
	}
}


void
fsp_protocol_handler(struct connection *conn)
{
	int fsp_pipe[2] = { -1, -1 };
	int header_pipe[2] = { -1, -1 };
	pid_t cpid;

	if (c_pipe(fsp_pipe) || c_pipe(header_pipe)) {
		int s_errno = errno;

		if (fsp_pipe[0] >= 0) close(fsp_pipe[0]);
		if (fsp_pipe[1] >= 0) close(fsp_pipe[1]);
		if (header_pipe[0] >= 0) close(header_pipe[0]);
		if (header_pipe[1] >= 0) close(header_pipe[1]);
		abort_connection(conn, connection_state_for_errno(s_errno));
		return;
	}
	conn->from = 0;
	conn->unrestartable = 1;
	find_auth(conn->uri); /* remember username and password */

	cpid = fork();
	if (cpid == -1) {
		int s_errno = errno;

		close(fsp_pipe[0]);
		close(fsp_pipe[1]);
		close(header_pipe[0]);
		close(header_pipe[1]);
		retry_connection(conn, connection_state_for_errno(s_errno));
		return;
	}

	if (!cpid) {
		dup2(fsp_pipe[1], 1);
		dup2(open("/dev/null", O_RDONLY), 0);
		dup2(header_pipe[1], 2);
		close(fsp_pipe[0]);
		close(header_pipe[0]);

		/* There may be outgoing data in stdio buffers
		 * inherited from the parent process.  The parent
		 * process is going to write this data, so the child
		 * process must not do that.  Closing the file
		 * descriptors ensures this.
		 *
		 * FIXME: If something opens more files and gets the
		 * same file descriptors and does not close them
		 * before exit(), then stdio may attempt to write the
		 * buffers to the wrong files.  This might happen for
		 * example if fsplib calls syslog().  */
		close_all_non_term_fd();
		do_fsp(conn);

	} else {
		struct read_buffer *buf2;

		conn->data_socket->fd = fsp_pipe[0];
		conn->socket->fd = header_pipe[0];
		set_nonblocking_fd(conn->data_socket->fd);
		set_nonblocking_fd(conn->socket->fd);
		close(fsp_pipe[1]);
		close(header_pipe[1]);
		buf2 = alloc_read_buffer(conn->socket);
		if (!buf2) {
			close_socket(conn->data_socket);
			close_socket(conn->socket);
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		read_from_socket(conn->socket, buf2,
				 connection_state(S_CONN), fsp_got_header);
	}
}
