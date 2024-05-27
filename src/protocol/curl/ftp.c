/* curl ftp protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>	/* For converting permissions to strings */
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <curl/curl.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/stat.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/curl/ftpes.h"
#include "protocol/curl/http.h"
#include "protocol/curl/sftp.h"
#include "protocol/ftp/parse.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef CONFIG_FTP

static char el_curlversion[256];

static void
init_ftpes(struct module *module)
{
	snprintf(el_curlversion, 255, "FTPES(%s)", curl_version());
	module->name = el_curlversion;
}

struct module ftpes_protocol_module = struct_module(
	/* name: */		N_("FTPES"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_ftpes,
	/* done: */		NULL
);

struct module sftp_protocol_module = struct_module(
	/* name: */		N_("SFTP"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


#define FTP_BUF_SIZE	16384


/* Types and structs */

struct ftpes_connection_info {
	CURL *easy;
	char *url;
	GlobalInfo *global;
	char error[CURL_ERROR_SIZE];
	int conn_state;
	int buf_pos;

	unsigned int dir:1;          /* Directory listing in progress */
	char ftp_buffer[FTP_BUF_SIZE];
};

/** How to format an FTP directory listing in HTML.  */
struct ftp_dir_html_format {
	/** Codepage used by C library functions such as strftime().
	 * If the FTP server sends non-ASCII bytes in file names or
	 * such, ELinks normally passes them straight through to the
	 * generated HTML, which will eventually be parsed using the
	 * codepage specified in the document.codepage.assume option.
	 * However, when ELinks itself generates strings with
	 * strftime(), it turns non-ASCII bytes into entity references
	 * based on libc_codepage, to make sure they become the right
	 * characters again.  */
	int libc_codepage;

	/** Nonzero if directories should be displayed in a different
	 * color.  From the document.browse.links.color_dirs option.  */
	int colorize_dir;

	/** The color of directories, in "#rrggbb" format.  This is
	 * initialized and used only if colorize_dir is nonzero.  */
	char dircolor[8];
};

/* Display directory entry formatted in HTML. */
static int
display_dir_entry(struct cache_entry *cached, off_t *pos, int *tries,
		  const struct ftp_dir_html_format *format,
		  struct ftp_file_info *ftp_info)
{
	struct string string;
	char permissions[10] = "---------";

	if (!init_string(&string)) return -1;

	add_char_to_string(&string, ftp_info->type);

	if (ftp_info->permissions) {
		mode_t p = ftp_info->permissions;

#define FTP_PERM(perms, buffer, flag, index, id) \
	if ((perms) & (flag)) (buffer)[(index)] = (id);

		FTP_PERM(p, permissions, S_IRUSR, 0, 'r');
		FTP_PERM(p, permissions, S_IWUSR, 1, 'w');
		FTP_PERM(p, permissions, S_IXUSR, 2, 'x');
		FTP_PERM(p, permissions, S_ISUID, 2, (p & S_IXUSR ? 's' : 'S'));

		FTP_PERM(p, permissions, S_IRGRP, 3, 'r');
		FTP_PERM(p, permissions, S_IWGRP, 4, 'w');
		FTP_PERM(p, permissions, S_IXGRP, 5, 'x');
		FTP_PERM(p, permissions, S_ISGID, 5, (p & S_IXGRP ? 's' : 'S'));

		FTP_PERM(p, permissions, S_IROTH, 6, 'r');
		FTP_PERM(p, permissions, S_IWOTH, 7, 'w');
		FTP_PERM(p, permissions, S_IXOTH, 8, 'x');
		FTP_PERM(p, permissions, S_ISVTX, 8, (p & 0001 ? 't' : 'T'));

#undef FTP_PERM

	}

	add_to_string(&string, permissions);
	add_char_to_string(&string, ' ');

	add_to_string(&string, "   1 ftp      ftp ");

	if (ftp_info->size != FTP_SIZE_UNKNOWN) {
		add_format_to_string(&string, "%12" OFF_PRINT_FORMAT " ",
				     (off_print_T) ftp_info->size);
	} else {
		add_to_string(&string, "           - ");
	}

#ifdef HAVE_STRFTIME
	if (ftp_info->mtime > 0) {
		time_t current_time = time(NULL);
		time_t when = ftp_info->mtime;
		struct tm *when_tm;
	       	char *fmt;
		/* LC_TIME=fi_FI.UTF_8 can generate "elo___ 31 23:59"
		 * where each _ denotes U+00A0 encoded as 0xC2 0xA0,
		 * thus needing a 19-byte buffer.  */
		char date[MAX_STR_LEN];
		int wr;

		if (ftp_info->local_time_zone)
			when_tm = localtime(&when);
		else
			when_tm = gmtime(&when);

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = gettext("%b %e  %Y");
		else
			fmt = gettext("%b %e %H:%M");

		wr = strftime(date, sizeof(date), fmt, when_tm);
		add_cp_html_to_string(&string, format->libc_codepage,
				      date, wr);
	} else
#endif
	add_to_string(&string, gettext("             "));
	/* TODO: Above, the number of spaces might not match the width
	 * of the string generated by strftime.  It depends on the
	 * locale.  So if ELinks knows the timestamps of some FTP
	 * files but not others, it may misalign the file names.
	 * Potential solutions:
	 * - Pad the strings to a compile-time fixed width.
	 *   Con: If we choose a width that suffices for all known
	 *   locales, then it will be stupidly large for most of them.
	 * - Generate an HTML table.
	 *   Con: Bloats the HTML source.
	 * Any solution chosen here should also be applied to the
	 * file: protocol handler.  */

	if (ftp_info->type == FTP_FILE_DIRECTORY && format->colorize_dir) {
		add_to_string(&string, "<font color=\"");
		add_to_string(&string, format->dircolor);
		add_to_string(&string, "\"><b>");
	}

	add_to_string(&string, "<a href=\"");
	encode_uri_string(&string, ftp_info->name.source, ftp_info->name.length, 0);
	if (ftp_info->type == FTP_FILE_DIRECTORY)
		add_char_to_string(&string, '/');
	add_to_string(&string, "\">");
	add_html_to_string(&string, ftp_info->name.source, ftp_info->name.length);
	add_to_string(&string, "</a>");

	if (ftp_info->type == FTP_FILE_DIRECTORY && format->colorize_dir) {
		add_to_string(&string, "</b></font>");
	}

	if (ftp_info->symlink.length) {
		add_to_string(&string, " -&gt; ");
		add_html_to_string(&string, ftp_info->symlink.source,
				ftp_info->symlink.length);
	}

	add_char_to_string(&string, '\n');

	if (add_fragment(cached, *pos, string.source, string.length)) *tries = 0;
	*pos += string.length;

	done_string(&string);
	return 0;
}

/* Get the next line of input and set *@len to the length of the line.
 * Return the number of newline characters at the end of the line or -1
 * if we must wait for more input. */
static int
ftp_get_line(struct cache_entry *cached, char *buf, int bufl,
             int last, int *len)
{
	char *newline;

	if (!bufl) return -1;

	newline = (char *)memchr(buf, ASCII_LF, bufl);

	if (newline) {
		*len = newline - buf;
		if (*len && buf[*len - 1] == ASCII_CR) {
			--*len;
			return 2;
		} else {
			return 1;
		}
	}

	if (last || bufl >= FTP_BUF_SIZE) {
		*len = bufl;
		return 0;
	}

	return -1;
}

/** Generate HTML for a line that was received from the FTP server but
 * could not be parsed.  The caller is supposed to have added a \<pre>
 * start tag.  (At the time of writing, init_directory_listing() was
 * used for that.)
 *
 * @return -1 if out of memory, or 0 if successful.  */
static int
ftp_add_unparsed_line(struct cache_entry *cached, off_t *pos, int *tries, 
		      const char *line, int line_length)
{
	int our_ret;
	struct string string;
	int frag_ret;

	our_ret = -1;	 /* assume out of memory if returning early */
	if (!init_string(&string)) goto out;
	if (!add_html_to_string(&string, line, line_length)) goto out;
	if (!add_char_to_string(&string, '\n')) goto out;

	frag_ret = add_fragment(cached, *pos, string.source, string.length);
	if (frag_ret == -1) goto out;
	*pos += string.length;
	if (frag_ret == 1) *tries = 0;

	our_ret = 0;		/* success */

out:
	done_string(&string);	/* safe even if init_string failed */
	return our_ret;
}

/** List a directory in html format.
 *
 * @return the number of bytes used from the beginning of @a buffer,
 * or -1 if out of memory.  */
static int
ftp_process_dirlist(struct cache_entry *cached, off_t *pos,
		    char *buffer, int buflen, int last,
		    int *tries, const struct ftp_dir_html_format *format)
{
	int ret = 0;

	while (1) {
		struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO;
		char *buf = buffer + ret;
		int bufl = buflen - ret;
		int line_length, eol;

		eol = ftp_get_line(cached, buf, bufl, last, &line_length);
		if (eol == -1)
			return ret;

		ret += line_length + eol;

		/* Process line whose end we've already found. */

		if (parse_ftp_file_info(&ftp_info, buf, line_length)) {
			int retv;

			if (ftp_info.name.source[0] == '.'
			    && (ftp_info.name.length == 1
				|| (ftp_info.name.length == 2
				    && ftp_info.name.source[1] == '.')))
				continue;

			retv = display_dir_entry(cached, pos, tries,
						 format, &ftp_info);
			if (retv < 0) {
				return ret;
			}

		} else {
			int retv = ftp_add_unparsed_line(cached, pos, tries,
							 buf, line_length);

			if (retv == -1) /* out of memory; propagate to caller */
				return retv;
		}
	}
}

static void ftpes_got_data(void *stream, void *buffer, size_t len);

static size_t
my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
	ftpes_got_data(stream, buffer, size * nmemb);
	return nmemb;
}

static void
done_ftpes(struct connection *conn)
{
	struct ftpes_connection_info *ftp = (struct ftpes_connection_info *)conn->info;

	if (!ftp || !ftp->easy) {
		return;
	}
	curl_multi_remove_handle(g.multi, ftp->easy);
	curl_easy_cleanup(ftp->easy);
}

static void
do_ftpes(struct connection *conn)
{
	struct ftpes_connection_info *ftp = (struct ftpes_connection_info *)mem_calloc(1, sizeof(*ftp));
	struct auth_entry *auth = find_auth(conn->uri);
	char *url;
	struct string u;
	CURL *curl;

	if (!ftp) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	conn->info = ftp;
	conn->done = done_ftpes;

	if (!conn->uri->datalen || conn->uri->data[conn->uri->datalen - 1] == '/') {
		ftp->dir = 1;
	}
	url = get_uri_string(conn->uri, URI_HOST | URI_PORT | URI_DATA);

	if (!url) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	if (!init_string(&u)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (conn->uri->protocol == PROTOCOL_FTP || conn->uri->protocol == PROTOCOL_FTPES) {
		add_to_string(&u, "ftp://");
	} else {
		add_to_string(&u, "sftp://");
	}

	if (auth) {
		add_to_string(&u, auth->user);
		add_char_to_string(&u, ':');
		add_to_string(&u, auth->password);
		add_char_to_string(&u, '@');
	}
	add_to_string(&u, url);

	if (!conn->uri->datalen || is_in_state(conn->state, S_RESTART)) {
		ftp->dir = 1;
		add_char_to_string(&u, '/');
	}
	mem_free(url);
	curl = curl_easy_init();

	if (curl) {
		CURLMcode rc;
		curl_off_t offset = conn->progress->start > 0 ? conn->progress->start : 0;
		conn->progress->seek = conn->from = offset;
		ftp->easy = curl;
		/* Define our callback to get called when there's data to be written */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
		/* Set a pointer to our struct to pass to the callback */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, conn);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, ftp->error);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, conn);
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, offset);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)get_opt_long("protocol.ftp.curl_max_recv_speed", NULL));
		curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t)get_opt_long("protocol.ftp.curl_max_send_speed", NULL));

		//curl_easy_setopt(curl, CURLOPT_XFERINFODATA, conn);
		//curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		/* enable TCP keep-alive for this transfer */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
		/* keep-alive idle time to 120 seconds */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
		/* interval time between keep-alive probes: 60 seconds */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

		/* We activate SSL and we require it for control */
		if (conn->uri->protocol == PROTOCOL_FTPES) {
			char *bundle = getenv("CURL_CA_BUNDLE");
			char *ciphers = get_opt_str("protocol.ftp.curl_tls13_ciphers", NULL);

			if (bundle) {
				curl_easy_setopt(curl, CURLOPT_CAINFO, bundle);
			}
			if (ciphers && *ciphers) {
				curl_easy_setopt(curl, CURLOPT_TLS13_CIPHERS, ciphers);
			}
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_CONTROL);
		}
///		curl_easy_setopt(curl, CURLOPT_STDERR, stream);
		/* Switch on full protocol/debug output */
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

		//fprintf(stderr, "Adding easy %p to multi %p (%s)\n", curl, g.multi, u.source);
		set_connection_state(conn, connection_state(S_TRANS));
		curl_easy_setopt(curl, CURLOPT_URL, u.source);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

		rc = curl_multi_add_handle(g.multi, curl);
		mcode_or_die("new_conn: curl_multi_add_handle", rc);
	}
	done_string(&u);
}

/* A read handler for conn->data_socket->fd.  This function reads
 * data from the FTP server, reformats it to HTML if it's a directory
 * listing, and adds the result to the cache entry.  */
static void
ftpes_got_data(void *stream, void *buf, size_t len)
{
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;
	struct ftpes_connection_info *ftp = (struct ftpes_connection_info *)conn->info;
	struct ftp_dir_html_format format;
	size_t len2 = len;
	size_t copied = 0;

	/* XXX: This probably belongs rather to connect.c ? */
	set_connection_timeout(conn);

	if (!conn->cached) {
		conn->cached = get_cache_entry(conn->uri);

		if (!conn->cached) {
out_of_mem:
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
	}

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (ftp->dir) {
		format.libc_codepage = get_cp_index("System");

		format.colorize_dir = get_opt_bool("document.browse.links.color_dirs", NULL);

		if (format.colorize_dir) {
			color_to_string(get_opt_color("document.colors.dirs", NULL),
					format.dircolor);
		}
	}

	if (ftp->dir && !conn->from) {
		struct string string;
		struct connection_state state;

		if (!conn->uri->data) {
			abort_connection(conn, connection_state(S_FTP_ERROR));
			return;
		}

		state = init_directory_listing(&string, conn->uri);
		if (!is_in_state(state, S_OK)) {
			abort_connection(conn, state);
			return;
		}

		add_fragment(conn->cached, conn->from, string.source, string.length);
		conn->from += string.length;

		done_string(&string);

		if (conn->uri->datalen) {
			struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO_ROOT;

			display_dir_entry(conn->cached, &conn->from, &conn->tries,
					  &format, &ftp_info);
		}

		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	}

	if (!ftp->dir && (len > 0)) {
		if (add_fragment(conn->cached, conn->from, buffer, len) == 1) {
			conn->tries = 0;
		}

		if ((conn->from == 0 || conn->progress->start > 0) && conn->est_length == -1) {
			curl_easy_getinfo(ftp->easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &conn->est_length);

			if (conn->est_length != -1) {
				conn->est_length += conn->from;
			}
		}
		conn->from += len;
		conn->received += len;
		return;
	}

again:
	len = len2;

	if (FTP_BUF_SIZE - ftp->buf_pos < len) {
		len = FTP_BUF_SIZE - ftp->buf_pos;
	}

	if (len2 > 0) {
		int proceeded;

		memcpy(ftp->ftp_buffer + ftp->buf_pos, buffer + copied, len);
		conn->received += len;
		copied += len;
		proceeded = ftp_process_dirlist(conn->cached,
						&conn->from,
						ftp->ftp_buffer,
						len + ftp->buf_pos,
						0, &conn->tries,
						&format);

		if (proceeded == -1) goto out_of_mem;

		ftp->buf_pos += len - proceeded;
		memmove(ftp->ftp_buffer, ftp->ftp_buffer + proceeded, ftp->buf_pos);

		len2 -= len;

		if (len2 <= 0) {
			return;
		}
		goto again;
	}

	if (ftp_process_dirlist(conn->cached, &conn->from,
				ftp->ftp_buffer, ftp->buf_pos, 1,
				&conn->tries, &format) == -1)
		goto out_of_mem;

#define ADD_CONST(str) { \
	add_fragment(conn->cached, conn->from, str, sizeof(str) - 1); \
	conn->from += (sizeof(str) - 1); }

	if (ftp->dir) ADD_CONST("</pre>\n<hr/>\n</body>\n</html>");
	abort_connection(conn, connection_state(S_OK));
}

static void
ftp_curl_handle_error(struct connection *conn, CURLcode res)
{
	if (res == CURLE_OK) {
		abort_connection(conn, connection_state(S_OK));
		return;
	}

	if (res == CURLE_REMOTE_FILE_NOT_FOUND || res == CURLE_SSH) {
		struct ftpes_connection_info *ftp = (struct ftpes_connection_info *)conn->info;

		if (ftp && !ftp->dir) {
			retry_connection(conn, connection_state(S_RESTART));
			return;
		}
	}
	abort_connection(conn, connection_state(S_CURL_ERROR - res));
}

void
ftpes_protocol_handler(struct connection *conn)
{
	if (g.multi) {
		do_ftpes(conn);
	}
}

void
sftp_protocol_handler(struct connection *conn)
{
	if (g.multi) {
		do_ftpes(conn);
	}
}
#endif