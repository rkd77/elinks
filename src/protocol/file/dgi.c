/* Internal "dgi" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "cookies/cookies.h"
#include "intl/libintl.h"
#include "mime/backend/common.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "osdep/types.h"
#include "protocol/common.h"
#include "protocol/file/dgi.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/env.h"
#include "util/string.h"

static union option_info dgi_options[] = {
	INIT_OPT_TREE("protocol.file", N_("DGI"),
		"dgi", OPT_ZERO,
		N_("Dos gateway interface specific options.")),

	NULL_OPTION_INFO,
};

struct dgi_entry {
	const char *name;
	const char *cmdline;
};

struct dgi_entry entries[] = {
	{ "cdplayer.dgi", "cdplayer.exe $s > $2" },
	NULL
};

struct module dgi_protocol_module = struct_module(
	/* name: */		N_("Dos Gateway Interface (DGI)"),
	/* options: */		dgi_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static struct dgi_entry *
find_dgi(const char *name)
{
	const char *last = strrchr(name, '/');
	struct dgi_entry *entry;

	if (last) {
		name = last + 1;
	}

	for (entry = entries; entry; entry++) {
		if (!entry->name) break;

		if (!strcmp(name, entry->name)) {
			return entry;
		}
	}

	return NULL;
}

static void
write_request_to_file(struct connection *conn, const char *filename)
{
	if (!conn->uri->post) {
		return;
	}
	FILE *f = fopen(filename, "wb");

	if (f) {
		char *post = conn->uri->post;
		char *postend = strchr(post, '\n');
		if (postend) {
			post = postend + 1;
		}
		struct http_connection_info *http = (struct http_connection_info *)conn->info;
		fwrite(post, 1, http->post.total_upload_length, f);
		fclose(f);
	}
}

enum dgi_state {
	NORMAL,
	DOLAR,
	PERCENT
};

static void
prepare_command(struct dgi_entry *entry, const char *query, struct string *cmd, char **inp, char **out)
{
	const char *ch;
	dgi_state state = NORMAL;

	for (ch = entry->cmdline; *ch; ch++) {
		switch (state) {
		case NORMAL:
		default:
			if (*ch == '$') {
				state = DOLAR;
			} else if (*ch == '%') {
				state = PERCENT;
			} else {
				add_char_to_string(cmd, *ch);
			}
			break;
		case DOLAR:
		case PERCENT:
			switch(*ch) {
				case 'e':
					break;
				case 's':
					if (query) {
						add_to_string(cmd, query);
					}
					state = NORMAL;
					break;
				case '1':
					*inp = tempname(NULL, "elinks", ".txt");
					if (*inp) {
						add_to_string(cmd, *inp);
					}
					state = NORMAL;
					break;
				case '2':
					*out = tempname(NULL, "elinks", ".htm");
					if (*out) {
						add_to_string(cmd, *out);
					}
					state = NORMAL;
					break;
				default:
					add_char_to_string(cmd, *ch);
					break;
			}
			break;
		}
	}
}

int
execute_dgi(struct connection *conn)
{
	char *script;
	struct dgi_entry *entry;
	struct string command;
	char *tempfilename = NULL;
	char *outputfilename = NULL;
	struct connection_state state = connection_state(S_OK);

	/* Not file referrer */
	if (conn->referrer && conn->referrer->protocol != PROTOCOL_FILE) {
		return 1;
	}

	script = get_uri_string(conn->uri, URI_PATH);
	if (!script) {
		state = connection_state(S_OUT_OF_MEM);
		abort_connection(conn, state);
		return 0;
	}

	entry = find_dgi(script);
	if (!entry) {
		mem_free(script);
		return 1;
	}

	if (!init_string(&command)) {
		mem_free(script);
		return 0;
	}

	char *query = get_uri_string(conn->uri, URI_QUERY);

	prepare_command(entry, query, &command, &tempfilename, &outputfilename);

	mem_free_if(query);

	if (tempfilename) {
		write_request_to_file(conn, tempfilename);
	}


	fprintf(stderr, "%s\n", command.source);


	system(command.source);
	done_string(&command);
	mem_free(script);

	if (tempfilename) {
		unlink(tempfilename);
	}

	if (!outputfilename) {
		state = connection_state(S_OK);
		abort_connection(conn, state);
		return 0;
	}

	struct string page;
	struct string name;

	if (!init_string(&name)) {
		unlink(outputfilename);
		return 0;
	}
	add_to_string(&name, outputfilename);
	state = read_encoded_file(&name, &page);
	unlink(outputfilename);
	done_string(&name);

	if (is_in_state(state, S_OK)) {
		struct cache_entry *cached;

		/* Try to add fragment data to the connection cache if either
		 * file reading or directory listing worked out ok. */
		cached = conn->cached = get_cache_entry(conn->uri);
		if (!conn->cached) {
			state = connection_state(S_OUT_OF_MEM);
		} else {
			add_fragment(cached, 0, page.source, page.length);
			conn->from += page.length;

			if (1) {
				char *head;

				/* If the system charset somehow
				 * changes after the directory listing
				 * has been generated, it should be
				 * parsed with the original charset.  */
				head = straconcat("\r\nContent-Type: text/html; charset=",
						  get_cp_mime_name(get_cp_index("System")),
						  "\r\n", (char *) NULL);

				/* Not so gracefully handle failed memory
				 * allocation. */
				if (!head)
					state = connection_state(S_OUT_OF_MEM);

				/* Setup directory listing for viewing. */
				mem_free_set(&cached->head, head);
			}
			done_string(&page);
		}
	}
	abort_connection(conn, state);
	return 0;
}
