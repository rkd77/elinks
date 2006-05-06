/* Internal "file" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "encoding/encoding.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/socket.h"
#include "protocol/common.h"
#include "protocol/file/cgi.h"
#include "protocol/file/file.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


static struct option_info file_options[] = {
	INIT_OPT_TREE("protocol", N_("Local files"),
		"file", 0,
		N_("Options specific to local browsing.")),

	INIT_OPT_BOOL("protocol.file", N_("Allow reading special files"),
		"allow_special_files", 0, 0,
		N_("Whether to allow reading from non-regular files.\n"
		"Note this can be dangerous; reading /dev/urandom or\n"
		"/dev/zero can ruin your day!")),

	INIT_OPT_BOOL("protocol.file", N_("Show hidden files in directory listing"),
		"show_hidden_files", 0, 1,
		N_("When set to false, files with name starting with a dot will be\n"
		   "hidden in local directories listing.")),

	INIT_OPT_BOOL("protocol.file", N_("Try encoding extensions"),
		"try_encoding_extensions", 0, 1,
		N_("When set, if we can't open a file named 'filename', we'll try\n"
		"to open 'filename' with some encoding extension appended\n"
		"(ie. 'filename.gz'); it depends on the supported encodings.")),

	NULL_OPTION_INFO,
};

struct module file_protocol_module = struct_module(
	/* name: */		N_("File"),
	/* options: */		file_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


/* Directory listing */

/* Based on the @entry attributes and file-/dir-/linkname is added to the @data
 * fragment. */
static inline void
add_dir_entry(struct directory_entry *entry, struct string *page,
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
			lnk = straconcat(" -> ", buf, NULL);
		}

		if (!stat(entry->name, &st) && S_ISDIR(st.st_mode))
			add_char_to_string(page, '/');
#endif
	}

	add_to_string(page, "\">");

	if (entry->attrib[0] == 'd' && *dircolor) {
		/* The <b> is for the case when use_document_colors is off. */
		string_concat(page, "<font color=\"", dircolor, "\"><b>", NULL);
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
static inline void
add_dir_entries(struct directory_entry *entries, unsigned char *dirpath,
		struct string *page)
{
	unsigned char dircolor[8];
	int dirpathlen = strlen(dirpath);
	int i;

	/* Setup @dircolor so it's easy to check if we should color dirs. */
	if (get_opt_bool("document.browse.links.color_dirs")) {
		color_to_string(get_opt_color("document.colors.dirs"),
				(unsigned char *) &dircolor);
	} else {
		dircolor[0] = 0;
	}

	for (i = 0; entries[i].name; i++) {
		add_dir_entry(&entries[i], page, dirpathlen, dircolor);
		mem_free(entries[i].attrib);
		mem_free(entries[i].name);
	}

	/* We may have allocated space for entries but added none. */
	mem_free_if(entries);
}

/* Generates a HTML page listing the content of @directory with the path
 * @dirpath. */
/* Returns a connection state. S_OK if all is well. */
static inline enum connection_state
list_directory(struct connection *conn, unsigned char *dirpath,
	       struct string *page)
{
	int show_hidden_files = get_opt_bool("protocol.file.show_hidden_files");
	struct directory_entry *entries;
	enum connection_state state;

	errno = 0;
	entries = get_directory_entries(dirpath, show_hidden_files);
	if (!entries) {
		if (errno) return -errno;
		return S_OUT_OF_MEM;
	}

	state = init_directory_listing(page, conn->uri);
	if (state != S_OK)
		return S_OUT_OF_MEM;

	add_dir_entries(entries, dirpath, page);

	if (!add_to_string(page, "</pre>\n<hr>\n</body>\n</html>\n")) {
		done_string(page);
		return S_OUT_OF_MEM;
	}

	return S_OK;
}

static void
read_special(struct connection *conn, int fd)
{
	struct read_buffer *rb;

	if (!init_http_connection_info(conn, 1, 0, 1)) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	conn->socket->fd = fd;
	rb = alloc_read_buffer(conn->socket);
	if (!rb) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	memcpy(rb->data, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n", 45);
	rb->length = 45;
	rb->freespace -= 45;

	conn->socket->state = SOCKET_END_ONCLOSE;
	read_from_socket(conn->socket, rb, S_SENT, http_got_header);
}

/* To reduce redundant error handling code [calls to abort_connection()]
 * most of the function is build around conditions that will assign the error
 * code to @state if anything goes wrong. The rest of the function will then just
 * do the necessary cleanups. If all works out we end up with @state being S_OK
 * resulting in a cache entry being created with the fragment data generated by
 * either reading the file content or listing a directory. */
void
file_protocol_handler(struct connection *connection)
{
	unsigned char *redirect_location = NULL;
	struct string page, name;
	enum connection_state state;
	unsigned char *type = NULL;

	if (get_cmd_opt_bool("anonymous")) {
		if (strcmp(connection->uri->string, "file:///dev/stdin")
		    || isatty(STDIN_FILENO)) {
			abort_connection(connection, S_FILE_ANONYMOUS);
			return;
		}
	}

#ifdef CONFIG_CGI
	if (!execute_cgi(connection)) return;
#endif /* CONFIG_CGI */

	/* This function works on already simplified file-scheme URI pre-chewed
	 * by transform_file_url(). By now, the function contains no hostname
	 * part anymore, possibly relative path is converted to an absolute one
	 * and uri->data is just the final path to file/dir we should try to
	 * show. */

	if (!init_string(&name)
	    || !add_uri_to_string(&name, connection->uri, URI_PATH)) {
		done_string(&name);
		abort_connection(connection, S_OUT_OF_MEM);
		return;
	}

	decode_uri_string(&name);

	if (file_is_dir(name.source)) {
		/* In order for global history and directory listing to
		 * function properly the directory url must end with a
		 * directory separator. */
		if (name.source[0] && !dir_sep(name.source[name.length - 1])) {
			redirect_location = "/";
			state = S_OK;
		} else {
			state = list_directory(connection, name.source, &page);
			type = "text/html";
		}

	} else {
		if (!strcmp(name.source, "/dev/stdin")) {
			done_string(&name);
			read_special(connection, STDIN_FILENO);
			return;
		}
		if (!strncmp(name.source, "/dev/fd/", 8)) {
			int fd = atoi(name.source + 8);

			if (fd > 0) {
				done_string(&name);
				read_special(connection, fd);
				return;
			}
		}
		state = read_encoded_file(&name, &page);
		/* FIXME: If state is now S_ENCODE_ERROR we should try loading
		 * the file undecoded. --jonas */
	}

	done_string(&name);

	if (state == S_OK) {
		struct cache_entry *cached;

		/* Try to add fragment data to the connection cache if either
		 * file reading or directory listing worked out ok. */
		cached = connection->cached = get_cache_entry(connection->uri);
		if (!connection->cached) {
			if (!redirect_location) done_string(&page);
			state = S_OUT_OF_MEM;

		} else if (redirect_location) {
			if (!redirect_cache(cached, redirect_location, 1, 0))
				state = S_OUT_OF_MEM;

		} else {
			add_fragment(cached, 0, page.source, page.length);
			connection->from += page.length;

			if (!cached->content_type) {
				unsigned char *ctype = null_or_stracpy(type);

				/* Not so gracefully handle failed memory
				 * allocation. */
				if (type && !ctype)
					state = S_OUT_OF_MEM;

				/* Setup file read or directory listing for
				 * viewing. */
				mem_free_set(&cached->content_type, ctype);
			}

			done_string(&page);
		}
	}

	abort_connection(connection, state);
}
