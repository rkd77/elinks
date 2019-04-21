/* SMB protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_LIBSMBCLIENT_H
#include <libsmbclient.h>
#endif
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
#include "protocol/smb/smb.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"

struct module smb_protocol_module = struct_module(
	/* name: */		N_("SMB"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static FILE *header_out, *data_out;

/* The child process generally does not bother to free the memory it
 * allocates.  When the process exits, the operating system will free
 * the memory anyway.  There is no point in changing this, because the
 * child process also inherits memory allocations from the parent
 * process, and it would be very cumbersome to free those.  */

static void
smb_error(struct connection_state error)
{
	if (is_system_error(error))
		fprintf(data_out, "S%d\n", (int) error.syserr);
	else
		fprintf(data_out, "I%d\n", (int) error.basic);
	fputs("text/x-error", header_out);
	exit(1);
}

/** First information such as permissions is gathered for each directory entry.
 * All entries are then sorted. */
static struct directory_entry *
get_smb_directory_entries(int dir, struct string *prefix)
{
	struct directory_entry *entries = NULL;
	
	int size = 0;
	struct smbc_dirent *entry;

	while ((entry = smbc_readdir(dir))) {
		struct stat st, *stp;
		struct directory_entry *new_entries;
		struct string attrib;
		struct string name;

		if (!strcmp(entry->name, "."))
			continue;

		new_entries = mem_realloc(entries, (size + 2) * sizeof(*new_entries));
		if (!new_entries) continue;
		entries = new_entries;

		if (!init_string(&attrib)) {
			continue;
		}

		if (!init_string(&name)) {
			done_string(&attrib);
			continue;
		}

		add_string_to_string(&name, prefix);
		add_to_string(&name, entry->name);

		stp = (smbc_stat(name.source, &st)) ? NULL : &st;

		stat_type(&attrib, stp);
		stat_mode(&attrib, stp);
		stat_links(&attrib, stp);
		stat_user(&attrib, stp);
		stat_group(&attrib, stp);
		stat_size(&attrib, stp);
		stat_date(&attrib, stp);

		entries[size].name = stracpy(entry->name);
		entries[size].attrib = attrib.source;
		done_string(&name);
		size++;
	}
	smbc_closedir(dir);

	if (!size) {
		/* We may have allocated space for entries but added none. */
		mem_free_if(entries);
		return NULL;
	}
	qsort(entries, size, sizeof(*entries), compare_dir_entries);
	memset(&entries[size], 0, sizeof(*entries));
	return entries;
}

static void
add_smb_dir_entry(struct directory_entry *entry, struct string *page,
	      int pathlen, unsigned char *dircolor)
{
	unsigned char *lnk = NULL;
	struct string html_encoded_name;
	struct string uri_encoded_name;

	if (!init_string(&html_encoded_name)) return;
	if (!init_string(&uri_encoded_name)) {
		done_string(&html_encoded_name);
		return;
	}

	encode_uri_string(&uri_encoded_name, entry->name + pathlen, -1, 1);
	add_html_to_string(&html_encoded_name, entry->name + pathlen,
			   strlen(entry->name) - pathlen);

	/* add_to_string(&fragment, &fragmentlen, "   "); */
	add_html_to_string(page, entry->attrib, strlen(entry->attrib));
	add_to_string(page, "<a href=\"");
	add_string_to_string(page, &uri_encoded_name);

	if (entry->attrib[0] == 'd') {
		add_char_to_string(page, '/');

#ifdef FS_UNIX_SOFTLINKS
	} else if (entry->attrib[0] == 'l') {
		struct stat st;
		unsigned char buf[MAX_STR_LEN];
		int readlen = readlink(entry->name, buf, MAX_STR_LEN);

		if (readlen > 0 && readlen != MAX_STR_LEN) {
			buf[readlen] = '\0';
			lnk = straconcat(" -> ", buf, (unsigned char *) NULL);
		}

		if (!stat(entry->name, &st) && S_ISDIR(st.st_mode))
			add_char_to_string(page, '/');
#endif
	}

	add_to_string(page, "\">");

	if (entry->attrib[0] == 'd' && *dircolor) {
		/* The <b> is for the case when use_document_colors is off. */
		string_concat(page, "<font color=\"", dircolor, "\"><b>",
			      (unsigned char *) NULL);
	}

	add_string_to_string(page, &html_encoded_name);
	done_string(&uri_encoded_name);
	done_string(&html_encoded_name);

	if (entry->attrib[0] == 'd' && *dircolor) {
		add_to_string(page, "</b></font>");
	}

	add_to_string(page, "</a>");
	if (lnk) {
		add_html_to_string(page, lnk, strlen(lnk));
		mem_free(lnk);
	}

	add_char_to_string(page, '\n');
}

/* First information such as permissions is gathered for each directory entry.
 * Finally the sorted entries are added to the @data->fragment one by one. */
static void
add_smb_dir_entries(struct directory_entry *entries, unsigned char *dirpath,
		struct string *page)
{
	unsigned char dircolor[8];
	int i;

	/* Setup @dircolor so it's easy to check if we should color dirs. */
	if (get_opt_bool("document.browse.links.color_dirs", NULL)) {
		color_to_string(get_opt_color("document.colors.dirs", NULL),
				(unsigned char *) &dircolor);
	} else {
		dircolor[0] = 0;
	}

	for (i = 0; entries[i].name; i++) {
		add_smb_dir_entry(&entries[i], page, 0, dircolor);
		mem_free(entries[i].attrib);
		mem_free(entries[i].name);
	}

	/* We may have allocated space for entries but added none. */
	mem_free_if(entries);
}

static void
smb_directory(int dir, struct string *prefix, struct uri *uri)
{
	struct string buf;
	struct directory_entry *entries;

	if (!is_in_state(init_directory_listing(&buf, uri), S_OK)) {
		smb_error(connection_state(S_OUT_OF_MEM));
	}

	fputs("text/html", header_out);
	fclose(header_out);

	entries = get_smb_directory_entries(dir, prefix);
	add_smb_dir_entries(entries, NULL, &buf);
	add_to_string(&buf, "</pre><hr/></body></html>\n");

	fputs(buf.source, data_out);
	done_string(&buf);
	exit(0);
}

static void
smb_auth(const char *srv, const char *shr, char *wg, int wglen, char *un,
	int unlen, char *pw, int pwlen)
{
	/* TODO */
}

#define READ_SIZE	4096

static void
do_smb(struct connection *conn)
{
	struct uri *uri = conn->uri;
	struct auth_entry *auth = find_auth(uri);
	struct string string;
	unsigned char *url;
	int dir;

	if ((uri->userlen && uri->passwordlen) || !auth) {
		url = get_uri_string(uri, URI_BASE);
	} else {
		unsigned char *uri_string = get_uri_string(uri, URI_HOST | URI_PORT | URI_DATA);

		if (!uri_string || !init_string(&string)) {
			smb_error(connection_state(S_OUT_OF_MEM));
		}
		/* Must URI-encode the username and password to avoid
		 * ambiguity if they contain "/:@" characters.
		 * Libsmbclient then decodes them again, and the
		 * server gets them as they were in auth->user and
		 * auth->password, i.e. as the user typed them in the
		 * auth dialog.  This implies that, if the username or
		 * password contains some characters or bytes that the
		 * user cannot directly type, then she cannot enter
		 * them.  If that becomes an actual problem, it should
		 * be fixed in the auth dialog, e.g. by providing a
		 * hexadecimal input mode.  */
		add_to_string(&string, "smb://");
		encode_uri_string(&string, auth->user, -1, 1);
		add_char_to_string(&string, ':');
		encode_uri_string(&string, auth->password, -1, 1);
		add_char_to_string(&string, '@');
		add_to_string(&string, uri_string);
		url = string.source;
	}

	if (!url) {
		smb_error(connection_state(S_OUT_OF_MEM));
	}
	if (smbc_init(smb_auth, 0)) {
		smb_error(connection_state_for_errno(errno));
	};


	dir = smbc_opendir(url);
	if (dir >= 0) {
		struct string prefix;

		init_string(&prefix);
		add_to_string(&prefix, url);
		add_char_to_string(&prefix, '/');
		smb_directory(dir, &prefix, conn->uri);
		done_string(&prefix);
	} else {
		const int errno_from_opendir = errno;
		char buf[READ_SIZE];
		struct stat sb;
		int r, res, fdout;
		int file = smbc_open(url, O_RDONLY, 0);

		if (file < 0) {
			/* If we're opening the list of shares without
			 * proper authentication, then smbc_opendir
			 * fails with EACCES and smbc_open fails with
			 * ENOENT.  In this case, return the EACCES so
			 * that the parent ELinks process will prompt
			 * for credentials.  */
			if (errno == ENOENT && errno_from_opendir == EACCES)
				errno = errno_from_opendir;
			smb_error(connection_state_for_errno(errno));
		}

		res = smbc_fstat(file, &sb);
		if (res) {
			smb_error(connection_state_for_errno(res));
		}
		/* filesize */
		fprintf(header_out, "%" OFF_PRINT_FORMAT,
			(off_print_T) sb.st_size);
		fclose(header_out);

		fdout = fileno(data_out);
		while ((r = smbc_read(file, buf, READ_SIZE)) > 0) {
			if (safe_write(fdout, buf, r) <= 0)
					break;
		}
		smbc_close(file);
		exit(0);
	}
}

#undef READ_SIZE

/* Kill the current connection and ask for a username/password for the next
 * try. */
static void
prompt_username_pw(struct connection *conn)
{
	add_auth_entry(conn->uri, "Samba", NULL, NULL, 0);
	abort_connection(conn, connection_state(S_OK));
}

static void
smb_got_error(struct socket *socket, struct read_buffer *rb)
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

	if (is_system_error(error) && error.syserr == EACCES)
		prompt_username_pw(conn);
	else
		abort_connection(conn, error);
}

static void
smb_got_data(struct socket *socket, struct read_buffer *rb)
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

	read_from_socket(socket, rb, connection_state(S_TRANS), smb_got_data);
}

static void
smb_got_header(struct socket *socket, struct read_buffer *rb)
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

					/* avoid error */
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
				 connection_state(S_CONN), smb_got_error);
	} else {
		read_from_socket(conn->data_socket, buf,
				 connection_state(S_CONN), smb_got_data);
	}
}

static void
close_all_fds_but_two(int header, int data)
{
	int n;
	int max = 1024;
#ifdef RLIMIT_NOFILE
	struct rlimit lim;

	if (!getrlimit(RLIMIT_NOFILE, &lim))
		max = lim.rlim_max;
#endif
	for (n = 3; n < max; n++) {
		if (n != header && n != data) close(n);
	}
}



void
smb_protocol_handler(struct connection *conn)
{
	int smb_pipe[2] = { -1, -1 };
	int header_pipe[2] = { -1, -1 };
	pid_t cpid;

	if (c_pipe(smb_pipe) || c_pipe(header_pipe)) {
		int s_errno = errno;

		if (smb_pipe[0] >= 0) close(smb_pipe[0]);
		if (smb_pipe[1] >= 0) close(smb_pipe[1]);
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

		close(smb_pipe[0]);
		close(smb_pipe[1]);
		close(header_pipe[0]);
		close(header_pipe[1]);
		retry_connection(conn, connection_state_for_errno(s_errno));
		return;
	}

	if (!cpid) {
		dup2(open("/dev/null", O_RDONLY), 0);
		close(1);
		close(2);
		data_out = fdopen(smb_pipe[1], "w");
		header_out = fdopen(header_pipe[1], "w");

		if (!data_out || !header_out) exit(1);

		close(smb_pipe[0]);
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
		 * example if libsmbclient calls syslog().  */

		close_all_fds_but_two(smb_pipe[1], header_pipe[1]);
		do_smb(conn);

	} else {
		struct read_buffer *buf2;

		conn->data_socket->fd = smb_pipe[0];
		conn->socket->fd = header_pipe[0];
		set_nonblocking_fd(conn->data_socket->fd);
		set_nonblocking_fd(conn->socket->fd);
		close(smb_pipe[1]);
		close(header_pipe[1]);
		buf2 = alloc_read_buffer(conn->socket);
		if (!buf2) {
			close_socket(conn->data_socket);
			close_socket(conn->socket);
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		read_from_socket(conn->socket, buf2,
				 connection_state(S_CONN), smb_got_header);
	}
}
