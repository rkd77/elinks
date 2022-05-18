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
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $a"),
		"a", OPT_ZERO, "",
		N_("Path to cache.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $b"),
		"b", OPT_ZERO, "",
		N_("Full name of bookmarks.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $c"),
		"c", OPT_ZERO, "",
		N_("Full name of cache index.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $d"),
		"d", OPT_ZERO, "",
		N_("Document name.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $e"),
		"e", OPT_ZERO, "",
		N_("Path to executable files.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $f"),
		"f", OPT_ZERO, "",
		N_("File browser arguments.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $g"),
		"g", OPT_ZERO, "",
		N_("IP address of 1st gateway.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $h"),
		"h", OPT_ZERO, "",
		N_("Full name of History file.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $i"),
		"i", OPT_ZERO, "",
		N_("Your IP address.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $j"),
		"j", OPT_ZERO, "",
		N_("DJPEG arguments.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $l"),
		"l", OPT_ZERO, "",
		N_("Last visited document.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $m"),
		"m", OPT_ZERO, "",
		N_("Path to mail.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $n"),
		"n", OPT_ZERO, "",
		N_("IP address of 1st nameserver.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $p"),
		"p", OPT_ZERO, "",
		N_("Host.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $q"),
		"q", OPT_ZERO, "",
		N_("Filename of query string (file created only "
		"when using this macro).")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $r"),
		"r", OPT_ZERO, "",
		N_("Horizontal resolution of screen.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $s"),
		"s", OPT_ZERO, "",
		N_("CGI compatible query string.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $t"),
		"t", OPT_ZERO, "",
		N_("Path for temporary files.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $u"),
		"u", OPT_ZERO, "",
		N_("URL of document.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $w"),
		"w", OPT_ZERO, "",
		N_("Download path.")),
	INIT_OPT_STRING("protocol.file.dgi", N_("Path $x"),
		"x", OPT_ZERO, "",
		N_("Netmask.")),
	NULL_OPTION_INFO,
};

struct dgi_entry {
	const char *name;
	const char *cmdline;
};

struct dgi_entry entries[] = {
	{ "cdplayer.dgi", "$ecdplayer.exe $s > $2" },
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
prepare_command(struct dgi_entry *entry, const char *query, struct string *cmd, char **inp, char **out, char **queryfile)
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
				case 'a':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.a", NULL));
					state = NORMAL;
					break;
				case 'b':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.b", NULL));
					state = NORMAL;
					break;
				case 'c':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.c", NULL));
					state = NORMAL;
					break;
				case 'd':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.d", NULL));
					state = NORMAL;
					break;
				case 'e':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.e", NULL));
					state = NORMAL;
					break;
				case 'f':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.f", NULL));
					state = NORMAL;
					break;
				case 'g':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.g", NULL));
					state = NORMAL;
					break;
				case 'h':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.h", NULL));
					state = NORMAL;
					break;
				case 'i':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.i", NULL));
					state = NORMAL;
					break;
				case 'j':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.j", NULL));
					state = NORMAL;
					break;
				case 'l':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.l", NULL));
					state = NORMAL;
					break;
				case 'm':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.m", NULL));
					state = NORMAL;
					break;
				case 'n':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.n", NULL));
					state = NORMAL;
					break;
				case 'p':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.p", NULL));
					state = NORMAL;
					break;
				case 'q':
					*queryfile = tempname(NULL, "elinks", ".txt");
					if (*queryfile) {
						add_to_string(cmd, *queryfile);
					}
					state = NORMAL;
					break;
				case 'r':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.r", NULL));
					state = NORMAL;
					break;
				case 's':
					if (query) {
						add_to_string(cmd, query);
					}
					state = NORMAL;
					break;
				case 't':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.t", NULL));
					state = NORMAL;
					break;
				case 'u':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.u", NULL));
					state = NORMAL;
					break;
				case 'w':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.w", NULL));
					state = NORMAL;
					break;
				case 'x':
					add_to_string(cmd, get_opt_str("protocol.file.dgi.x", NULL));
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
					state = NORMAL;
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
	char *queryfile = NULL;
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

	prepare_command(entry, query, &command, &tempfilename, &outputfilename, &queryfile);

	mem_free_if(query);

	if (tempfilename) {
		write_request_to_file(conn, tempfilename);
	}

	if (queryfile) {
		FILE *f = fopen(queryfile, "wb");
		if (f) {
			fputs(query, f);
			fclose(f);
		}
	}

	fprintf(stderr, "%s\n", command.source);


	system(command.source);
	done_string(&command);
	mem_free(script);

	if (tempfilename) {
		unlink(tempfilename);
	}

	if (queryfile) {
		unlink(queryfile);
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
