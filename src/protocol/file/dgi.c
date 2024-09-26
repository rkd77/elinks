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
#include "mime/backend/dgi.h"
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
#include "util/qs_parse/qs_parse.h"
#include "util/string.h"

struct module dgi_protocol_module = struct_module(
	/* name: */		N_("DGI"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL,
	/* getname: */	NULL
);


static struct mime_handler *
find_dgi(const char *name)
{
	const char *last = strrchr(name, '/');
	struct mime_handler *handler;

	if (last) {
		name = last + 1;
	}

	struct string dtype;

	if (!init_string(&dtype)) {
		return NULL;
	}

	add_to_string(&dtype, "file/");
	add_to_string(&dtype, name);
	handler = get_mime_handler_dgi(dtype.source, 0);
	done_string(&dtype);

	return handler;
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
	PERCENT,
	LEFT_BRACKET
};

static void
prepare_command(char *program, const char *filename, const char *query, char *inpext, char *outext, struct string *cmd, char **inp, char **out, char **queryfile)
{
	const char *ch;
	enum dgi_state state = NORMAL;

	for (ch = program; *ch; ch++) {
		switch (state) {
		case NORMAL:
		default:
			if (*ch == '$') {
				state = DOLAR;
			} else if (*ch == '%') {
				state = PERCENT;
			} else if (*ch == '[') {
				state = LEFT_BRACKET;
			} else {
				add_char_to_string(cmd, *ch);
			}
			break;
		case LEFT_BRACKET:
			switch (*ch) {
			case ']':
				state = NORMAL;
				break;
			default:
				break;
			}
			break;
		case DOLAR:
		case PERCENT:
			switch (*ch) {
				case 'a':
					add_to_string(cmd, get_opt_str("mime.dgi.a", NULL));
					state = NORMAL;
					break;
				case 'b':
					add_to_string(cmd, get_opt_str("mime.dgi.b", NULL));
					state = NORMAL;
					break;
				case 'c':
					add_to_string(cmd, get_opt_str("mime.dgi.c", NULL));
					state = NORMAL;
					break;
				case 'd':
					add_to_string(cmd, get_opt_str("mime.dgi.d", NULL));
					state = NORMAL;
					break;
				case 'e':
					add_to_string(cmd, get_opt_str("mime.dgi.e", NULL));
					state = NORMAL;
					break;
				case 'f':
					add_to_string(cmd, get_opt_str("mime.dgi.f", NULL));
					state = NORMAL;
					break;
				case 'g':
					add_to_string(cmd, get_opt_str("mime.dgi.g", NULL));
					state = NORMAL;
					break;
				case 'h':
					add_to_string(cmd, get_opt_str("mime.dgi.h", NULL));
					state = NORMAL;
					break;
				case 'i':
					add_to_string(cmd, get_opt_str("mime.dgi.i", NULL));
					state = NORMAL;
					break;
				case 'j':
					add_to_string(cmd, get_opt_str("mime.dgi.j", NULL));
					state = NORMAL;
					break;
				case 'l':
					add_to_string(cmd, get_opt_str("mime.dgi.l", NULL));
					state = NORMAL;
					break;
				case 'm':
					add_to_string(cmd, get_opt_str("mime.dgi.m", NULL));
					state = NORMAL;
					break;
				case 'n':
					add_to_string(cmd, get_opt_str("mime.dgi.n", NULL));
					state = NORMAL;
					break;
				case 'p':
					add_to_string(cmd, get_opt_str("mime.dgi.p", NULL));
					state = NORMAL;
					break;
				case 'q':
					*queryfile = tempname(NULL, "elinks", inpext);
					if (*queryfile) {
						add_to_string(cmd, *queryfile);
					}
					state = NORMAL;
					break;
				case 'r':
					add_to_string(cmd, get_opt_str("mime.dgi.r", NULL));
					state = NORMAL;
					break;
				case 's':
					if (query) {
						add_to_string(cmd, query);
					}
					state = NORMAL;
					break;
				case 't':
					add_to_string(cmd, get_opt_str("mime.dgi.t", NULL));
					state = NORMAL;
					break;
				case 'u':
					add_to_string(cmd, get_opt_str("mime.dgi.u", NULL));
					state = NORMAL;
					break;
				case 'w':
					add_to_string(cmd, get_opt_str("mime.dgi.w", NULL));
					state = NORMAL;
					break;
				case 'x':
					add_to_string(cmd, get_opt_str("mime.dgi.x", NULL));
					state = NORMAL;
					break;
				case '1':
					if (filename) {
						add_to_string(cmd, filename);
					} else {
						*inp = tempname(NULL, "elinks", inpext);
						if (*inp) {
							add_to_string(cmd, *inp);
						}
					}
					state = NORMAL;
					break;
				case '2':
					*out = tempname(NULL, "elinks", outext);
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

void
dgi_protocol_handler(struct connection *conn)
{
#define NUMKVPAIRS 16
	char *query;
	struct connection_state state = connection_state(S_OK);

	int i;
	char *kvpairs[NUMKVPAIRS];
	char *command=NULL;
	char *filename=NULL;
	char *inpext=NULL;
	char *outext=NULL;
	char *del = NULL;

	struct string command_str;
	char *tempfilename = NULL;
	char *outputfilename = NULL;

	/* security checks */
	if (!conn->referrer || conn->referrer->protocol != PROTOCOL_DGI) {
		abort_connection(conn, connection_state(S_BAD_URL));
		return;
	}

	if (!init_string(&command_str)) {
		state = connection_state(S_OUT_OF_MEM);
		abort_connection(conn, state);
		return;
	}

	query = get_uri_string(conn->uri, URI_QUERY);

	if (query) {
		i = qs_parse(query, kvpairs, 16);
		command = qs_k2v("command", kvpairs, i);
		filename = qs_k2v("filename", kvpairs, i);
		inpext = qs_k2v("inpext", kvpairs, i);
		outext = qs_k2v("outext", kvpairs, i);
		del = qs_k2v("delete", kvpairs, i);
	}
	prepare_command(command, filename, NULL, inpext, outext, &command_str, &tempfilename, &outputfilename, NULL);

	(void)!system(command_str.source);
	done_string(&command_str);

	if (del) {
		unlink(filename);
	}

	if (tempfilename) {
		unlink(tempfilename);
	}

	if (!outputfilename) {
		state = connection_state(S_OK);
		abort_connection(conn, state);
		mem_free_if(query);
		return;
	}

	struct string page;
	struct string name;

	if (!init_string(&name)) {
		unlink(outputfilename);
		mem_free_if(query);
		return;
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
				char *otype = NULL;

				if (outext) {
					otype = get_extension_content_type(outext);
				}
				if (!otype) {
					otype = stracpy("text/html");
				}

				/* If the system charset somehow
				 * changes after the directory listing
				 * has been generated, it should be
				 * parsed with the original charset.  */
				head = straconcat("\r\nContent-Type: ", otype, "; charset=",
						  get_cp_mime_name(get_cp_index("System")),
						  "\r\n", (char *) NULL);

				mem_free_if(otype);

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
	mem_free_if(query);
	abort_connection(conn, state);
}

int
execute_dgi(struct connection *conn)
{
	char *script;
	struct mime_handler *handler;
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

	handler = find_dgi(script);
	if (!handler) {
		mem_free(script);
		return 1;
	}

	if (!init_string(&command)) {
		mem_free(script);
		return 0;
	}

	char *query = get_uri_string(conn->uri, URI_QUERY);

	prepare_command(handler->program, NULL, query, handler->inpext, handler->outext, &command, &tempfilename, &outputfilename, &queryfile);

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
	(void)!system(command.source);
	done_string(&command);
	mem_free(script);

	if (tempfilename) {
		unlink(tempfilename);
	}

	if (queryfile) {
		unlink(queryfile);
	}


	if (!outputfilename) {
		mem_free(handler);
		state = connection_state(S_OK);
		abort_connection(conn, state);
		return 0;
	}

	struct string page;
	struct string name;

	if (!init_string(&name)) {
		unlink(outputfilename);
		mem_free(handler);
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
				char *otype = NULL;

				if (handler->outext) {
					otype = get_extension_content_type(handler->outext);
				}
				if (!otype) {
					otype = stracpy("text/html");
				}

				/* If the system charset somehow
				 * changes after the directory listing
				 * has been generated, it should be
				 * parsed with the original charset.  */
				head = straconcat("\r\nContent-Type: ", otype, "; charset=",
						  get_cp_mime_name(get_cp_index("System")),
						  "\r\n", (char *) NULL);

				mem_free_if(otype);

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
	mem_free(handler);
	abort_connection(conn, state);
	return 0;
}
