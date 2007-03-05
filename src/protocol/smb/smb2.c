/* SMB protocol implementation */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

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
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"

/* These options are not used. */
#if 0
struct option_info smb_options[] = {
	INIT_OPT_TREE("protocol", N_("SMB"),
		"smb", 0,
		N_("SAMBA specific options.")),

	INIT_OPT_STRING("protocol.smb", N_("Credentials"),
		"credentials", 0, "",
		N_("Credentials file passed to smbclient via -A option.")),

	NULL_OPTION_INFO,
};
#endif

struct module smb_protocol_module = struct_module(
	/* name: */		N_("SMB"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static void
smb_error(int error)
{
	fprintf(stderr, "text/x-error");
	printf("%d\n", error);
	exit(1);
}

static int
compare(const void *a, const void *b)
{
	const struct smbc_dirent **da = (const struct smbc_dirent **)a;
	const struct smbc_dirent **db = (const struct smbc_dirent **)b;
	int res = (*da)->smbc_type - (*db)->smbc_type;

	if (res) {
		return res;
	}
	return strcmp((*da)->name, (*db)->name);
}

static void
smb_add_link(struct string *string, const struct smbc_dirent *entry,
	const unsigned char *text, const unsigned char dircolor[])
{
	struct string uri_string;

	if (!init_string(&uri_string)) return;
	encode_uri_string(&uri_string, entry->name, entry->namelen, 0);

	add_to_string(string, "<a href=\"");
	add_html_to_string(string, uri_string.source, uri_string.length);
	done_string(&uri_string);

	add_to_string(string, "\">");
	if (*dircolor) {
		add_to_string(string, "<font color=\"");
		add_to_string(string, dircolor);
		add_to_string(string, "\"><b>");
	}
	add_html_to_string(string, entry->name, entry->namelen);
	if (*dircolor) {
		add_to_string(string, "</b></font>");
	}
	add_to_string(string, "</a>");
	if (text) add_to_string(string, text);
}

static void
display_entry(const struct smbc_dirent *entry, const unsigned char dircolor[])
{
	static const unsigned char zero = '\0';
	struct string string;

	if (!init_string(&string)) return;

	switch (entry->smbc_type) {
	case SMBC_WORKGROUP:
		smb_add_link(&string, entry, " WORKGROUP ", dircolor);
		break;
	case SMBC_SERVER:
		smb_add_link(&string, entry, " SERVER ", dircolor);
		if (entry->comment) {
			add_html_to_string(&string, entry->comment, entry->commentlen);
		}
		break;
	case SMBC_FILE_SHARE:
		smb_add_link(&string, entry, " FILE SHARE ", dircolor);
		if (entry->comment) {
			add_html_to_string(&string, entry->comment, entry->commentlen);
		}
		break;
	case SMBC_PRINTER_SHARE:
		add_html_to_string(&string, entry->name, entry->namelen);
		add_to_string(&string, " PRINTER ");
		if (entry->comment) {
			add_html_to_string(&string, entry->comment, entry->commentlen);
		}
		break;
	case SMBC_COMMS_SHARE:
		add_bytes_to_string(&string, entry->name, entry->namelen);
		add_to_string(&string, " COMM");
		break;
	case SMBC_IPC_SHARE:
		add_bytes_to_string(&string, entry->name, entry->namelen);
		add_to_string(&string, " IPC");
		break;
	case SMBC_DIR:
		smb_add_link(&string, entry, NULL, dircolor);
		break;
	case SMBC_LINK:
		smb_add_link(&string, entry, " Link", &zero);
		break;
	case SMBC_FILE:
		smb_add_link(&string, entry, NULL, &zero);
		break;
	default:
		/* unknown type */
		break;
	}
	puts(string.source);
	done_string(&string);
}

static void
sort_and_display_entries(int dir, const unsigned char dircolor[])
{
	struct smbc_dirent *fentry, **table = NULL;
	int size = 0;
	int i;

	while ((fentry = smbc_readdir(dir))) {
		struct smbc_dirent **new_table, *new_entry;
		unsigned int commentlen = fentry->commentlen;
		unsigned int namelen = fentry->namelen;

		if (!strcmp(fentry->name, "."))
			continue;

		/* In libsmbclient 3.0.10, @smbc_dirent.namelen and
		 * @smbc_dirent.commentlen include the null characters
		 * (tested with GDB).  In libsmbclient 3.0.24, they
		 * don't.  This is related to Samba bug 3030.  Adjust
		 * the lengths to exclude the null characters, so that
		 * other code need not care.
		 *
		 * Make all changes to local copies rather than
		 * directly to *@fentry, so that there's no chance of
		 * ELinks messing up whatever mechanism libsmbclient
		 * will use to free @fentry.  */
		if (commentlen > 0 && fentry->comment[commentlen - 1] == '\0')
			commentlen--;
		if (namelen > 0 && fentry->name[namelen - 1] == '\0')
			namelen--;

		/* libsmbclient seems to place the struct smbc_dirent,
		 * the name string, and the comment string all in one
		 * block of memory, which then is smbc_dirent.dirlen
		 * bytes long.  This has however not been really
		 * documented, so ELinks should not assume copying
		 * fentry->dirlen bytes will copy the comment too.
		 * Yet, it would be wasteful to copy both dirlen bytes
		 * and then the comment string separately.  What we do
		 * here is ignore fentry->dirlen and recompute the
		 * size based on namelen.  */
		new_entry = (struct smbc_dirent *)
			memacpy((const unsigned char *) fentry,
				offsetof(struct smbc_dirent, name)
				+ namelen); /* memacpy appends '\0' */
		if (!new_entry)
			continue;
		new_entry->namelen = namelen;
		new_entry->commentlen = commentlen;
		if (fentry->comment)
			new_entry->comment = memacpy(fentry->comment, commentlen);
		if (!new_entry->comment)
			new_entry->commentlen = 0;

		new_table = mem_realloc(table, (size + 1) * sizeof(*table));
		if (!new_table)
			continue;
		table = new_table;
		table[size] = new_entry;
		size++;
	}
	qsort(table, size, sizeof(*table),
	 (int (*)(const void *, const void *)) compare);

	for (i = 0; i < size; i++) {
		display_entry(table[i], dircolor);
	}
}

static void
smb_directory(int dir, struct uri *uri)
{
	struct string buf;
	unsigned char dircolor[8] = "";

	if (init_directory_listing(&buf, uri) != S_OK) {
		smb_error(-S_OUT_OF_MEM);
	}

	fprintf(stderr, "text/html");
	fclose(stderr);

	puts(buf.source);

	if (get_opt_bool("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
	}

	sort_and_display_entries(dir, dircolor);
	puts("</pre><hr/></body></html>");
	smbc_closedir(dir);
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
			smb_error(-S_OUT_OF_MEM);
		}
		add_to_string(&string, "smb://");
		add_to_string(&string, auth->user);
		add_char_to_string(&string, ':');
		add_to_string(&string, auth->password);
		add_char_to_string(&string, '@');
		add_to_string(&string, uri_string);
		url = string.source;
	}

	if (!url) {
		smb_error(-S_OUT_OF_MEM);
	}
	if (smbc_init(smb_auth, 0)) {
		smb_error(errno);
	};
	decode_uri(url);

	dir = smbc_opendir(url);
	if (dir >= 0) {
		smb_directory(dir, conn->uri);
	} else {
		const int errno_from_opendir = errno;
		char buf[READ_SIZE];
		struct stat sb;
		int r, res;
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
			smb_error(errno);
		}

		res = smbc_fstat(file, &sb);
		if (res) {
			smb_error(res);
		}
		/* filesize */
		fprintf(stderr, "%" OFF_T_FORMAT, sb.st_size);
		fclose(stderr);

		while ((r = smbc_read(file, buf, READ_SIZE)) > 0) {
			if (safe_write(STDOUT_FILENO, buf, r) <= 0)
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
	abort_connection(conn, S_OK);
}

static void
smb_got_error(struct socket *socket, struct read_buffer *rb)
{
	int len = rb->length;
	struct connection *conn = socket->conn;
	int error;

	if (len < 0) {
		abort_connection(conn, -errno);
		return;
	}

	rb->data[len] = '\0';
	error = atoi(rb->data);
	kill_buffer_data(rb, len);
	switch (error) {
	case EACCES:
		prompt_username_pw(conn);
		break;
	default:
		abort_connection(conn, -error);
		break;
	}
}

static void
smb_got_data(struct socket *socket, struct read_buffer *rb)
{
	int len = rb->length;
	struct connection *conn = socket->conn;

	if (len < 0) {
		abort_connection(conn, -errno);
		return;
	}

	if (!len) {
		abort_connection(conn, S_OK);
		return;
	}

	socket->state = SOCKET_END_ONCLOSE;
	conn->received += len;
	if (add_fragment(conn->cached, conn->from, rb->data, len) == 1)
		conn->tries = 0;
	conn->from += len;
	kill_buffer_data(rb, len);

	read_from_socket(socket, rb, S_TRANS, smb_got_data);
}

static void
smb_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct read_buffer *buf;
	int error = 0;

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		close(socket->fd);
		close(conn->data_socket->fd);
		abort_connection(conn, S_OUT_OF_MEM);
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
					conn->est_length = (off_t)atoi(ctype);
#endif
					mem_free(ctype);

					/* avoid error */
					if (!conn->est_length) {
						abort_connection(conn, S_OK);
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
		close(socket->fd);
		close(conn->data_socket->fd);
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	if (error) {
		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
		read_from_socket(conn->data_socket, buf, S_CONN, smb_got_error);
	} else {
		read_from_socket(conn->data_socket, buf, S_CONN, smb_got_data);
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
		abort_connection(conn, -s_errno);
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
		retry_connection(conn, -s_errno);
		return;
	}

	if (!cpid) {
		dup2(smb_pipe[1], 1);
		dup2(open("/dev/null", O_RDONLY), 0);
		dup2(header_pipe[1], 2);
		close(smb_pipe[0]);
		close(header_pipe[0]);

		close_all_non_term_fd();
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
			close(smb_pipe[0]);
			close(header_pipe[0]);
			abort_connection(conn, S_OUT_OF_MEM);
			return;
		}
		read_from_socket(conn->socket, buf2, S_CONN, smb_got_header);
	}
}
