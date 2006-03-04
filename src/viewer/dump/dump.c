/* Support for dumping to the file on startup (w/o bfu) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> /* NetBSD flavour */
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/document.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
#include "main/main.h"
#include "network/connection.h"
#include "network/state.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "terminal/color.h"
#include "terminal/hardio.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


static int dump_pos;
static struct download dump_download;
static int dump_redir_count = 0;

static int dump_to_file_16(struct document *document, int fd);
static int dump_to_file_256(struct document *document, int fd);

/* This dumps the given @cached's source onto @fd nothing more. It returns 0 if it
 * all went fine and 1 if something isn't quite right and we should terminate
 * ourselves ASAP. */
static int
dump_source(int fd, struct download *download, struct cache_entry *cached)
{
	struct fragment *frag;

	if (!cached) return 0;

nextfrag:
	foreach (frag, cached->frag) {
		int d = dump_pos - frag->offset;
		int l, w;

		if (d < 0 || frag->length <= d)
			continue;

		l = frag->length - d;
		w = hard_write(fd, frag->data + d, l);

		if (w != l) {
			detach_connection(download, dump_pos);

			if (w < 0)
				ERROR(gettext("Can't write to stdout: %s"),
				      (unsigned char *) strerror(errno));
			else
				ERROR(gettext("Can't write to stdout."));

			program.retval = RET_ERROR;
			return 1;
		}

		dump_pos += w;
		detach_connection(download, dump_pos);
		goto nextfrag;
	}

	return 0;
}

/* This dumps the given @cached's formatted output onto @fd. */
static void
dump_formatted(int fd, struct download *download, struct cache_entry *cached)
{
	struct document_options o;
	struct document_view formatted;
	struct view_state vs;
	int width;

	if (!cached) return;

	memset(&formatted, 0, sizeof(formatted));

	init_document_options(&o);
	width = get_opt_int("document.dump.width");
	set_box(&o.box, 0, 1, width, DEFAULT_TERMINAL_HEIGHT);

	o.cp = get_opt_codepage("document.dump.codepage");
	o.color_mode = get_opt_int("document.dump.color_mode");
	o.plain = 0;
	o.frames = 0;
	o.links_numbering = get_opt_bool("document.dump.numbering");

	init_vs(&vs, cached->uri, -1);

	render_document(&vs, &formatted, &o);
	switch(o.color_mode) {
	case COLOR_MODE_DUMP:
	case COLOR_MODE_MONO: /* FIXME: inversion */	
		dump_to_file(formatted.document, fd);
		break;
	case COLOR_MODE_16:
		dump_to_file_16(formatted.document, fd);
		break;
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
		dump_to_file_256(formatted.document, fd);
		break;
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
		dump_to_file_256(formatted.document, fd);
		break;
#endif
	default:
		break;
	}

	detach_formatted(&formatted);
	destroy_vs(&vs, 1);
}

static unsigned char *
subst_url(unsigned char *str, struct string *url)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*str) {
		int p;

		for (p = 0; str[p] && str[p] != '%' && str[p] != '\\'; p++);

		add_bytes_to_string(&string, str, p);
		str += p;

		if (*str == '\\') {
			unsigned char ch;

			str++;
			switch (*str) {
				case 'f':
					ch = '\f';
					break;
				case 'n':
					ch = '\n';
					break;
				case 't':
					ch = '\t';
					break;
				default:
					ch = *str;
			}
			if (*str) {
				add_char_to_string(&string, ch);
				str++;
			}
			continue;

		} else if (*str != '%') {
			break;
		}

		str++;
		switch (*str) {
			case 'u':
				if (url) add_string_to_string(&string, url);
				break;
		}

		if (*str) str++;
	}

	return string.source;
}

static void
dump_print(unsigned char *option, struct string *url)
{
	unsigned char *str = get_opt_str(option);

	if (str) {
		unsigned char *realstr = subst_url(str, url);

		if (realstr) {
			printf("%s", realstr);
			fflush(stdout);
			mem_free(realstr);
		}
	}
}

static void
dump_loading_callback(struct download *download, void *p)
{
	struct cache_entry *cached = download->cached;
	int fd = get_output_handle();

	if (fd == -1) return;
	if (cached && cached->redirect && dump_redir_count++ < MAX_REDIRECTS) {
		struct uri *uri = cached->redirect;

		cancel_download(download, 0);

		load_uri(uri, cached->uri, download, PRI_MAIN, 0, -1);
		return;
	}

	if (is_in_queued_state(download->state)) return;

	if (get_cmd_opt_bool("dump")) {
		if (is_in_transfering_state(download->state))
			return;

		dump_formatted(fd, download, cached);

	} else {
		if (dump_source(fd, download, cached) > 0)
			goto terminate;

		if (is_in_progress_state(download->state))
			return;

	}

	if (download->state != S_OK) {
		usrerror(get_state_message(download->state, NULL));
		program.retval = RET_ERROR;
		goto terminate;
	}

terminate:
	program.terminate = 1;
	dump_next(NULL);
}

static void
dump_start(unsigned char *url)
{
	unsigned char *wd = get_cwd();
	struct uri *uri = get_translated_uri(url, wd);

	mem_free_if(wd);

	if (!uri || get_protocol_external_handler(NULL, uri)) {
		usrerror(gettext("URL protocol not supported (%s)."), url);
		goto terminate;
	}

	dump_download.callback = (download_callback_T *) dump_loading_callback;
	dump_pos = 0;

	if (load_uri(uri, NULL, &dump_download, PRI_MAIN, 0, -1)) {
terminate:
		dump_next(NULL);
		program.terminate = 1;
		program.retval = RET_SYNTAX;
	}

	if (uri) done_uri(uri);
}

void
dump_next(struct list_head *url_list)
{
	static INIT_LIST_HEAD(todo_list);
	static INIT_LIST_HEAD(done_list);
	struct string_list_item *item;

	if (url_list) {
		/* Steal all them nice list items but keep the same order */
		while (!list_empty(*url_list)) {
			item = url_list->next;
			del_from_list(item);
			add_to_list_end(todo_list, item);
		}
	}

	/* Dump each url list item one at a time */
	if (!list_empty(todo_list)) {
		static int first = 1;

		program.terminate = 0;

		item = todo_list.next;
		del_from_list(item);
		add_to_list(done_list, item);

		if (!first) {
			dump_print("document.dump.separator", NULL);
		} else {
			first = 0;
		}

		dump_print("document.dump.header", &item->string);
		dump_start(item->string.source);
		/* XXX: I think it ought to print footer at the end of
		 * the whole dump (not only this file). Testing required.
		 * --pasky */
		dump_print("document.dump.footer", &item->string);

	} else {
		free_string_list(&done_list);
		program.terminate = 1;
	}
}

/* Using this function in dump_to_file() is unfortunately slightly slower than
 * the current code.  However having this here instead of in the scripting
 * backends is better. */
struct string *
add_document_to_string(struct string *string, struct document *document)
{
	int y;

	assert(string && document);
	if_assert_failed return NULL;

	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			struct screen_char *pos = &document->data[y].chars[x];
			unsigned char data = pos->data;
			unsigned int frame = (pos->attr & SCREEN_ATTR_FRAME);

			if (!isscreensafe(data)) {
				white++;
				continue;
			} else {
				if (frame && data >= 176 && data < 224)
					data = frame_dumb[data - 176];

				if (data <= ' ') {
					/* Count spaces. */
					white++;
				} else {
					/* Print spaces if any. */
					if (white) {
						add_xchar_to_string(string, ' ', white);
						white = 0;
					}
					add_char_to_string(string, data);
				}
			}
		}

		add_char_to_string(string, '\n');
	}

	return string;
}

#define D_BUF	65536

static int
write_char(unsigned char c, int fd, unsigned char *buf, int *bptr)
{
	buf[(*bptr)++] = c;
	if ((*bptr) >= D_BUF) {
		if (hard_write(fd, buf, (*bptr)) != (*bptr))
			return -1;
		(*bptr) = 0;
	}

	return 0;
}

static int
write_color_16(unsigned char color, int fd, unsigned char *buf, int *bptr)
{
	unsigned char bufor[] = "\033[0;30;40m";
	unsigned char *data = bufor;
	int background = (color >> 4) & 7;
	int foreground = color & 7;

	bufor[5] += foreground;
	if (background)	bufor[8] += background;
	else {
		bufor[6] = 'm';
		bufor[7] = '\0';
	}
	while(*data) {
		if (write_char(*data++, fd, buf, bptr)) return -1;
	}
	return 0;
}


static int
dump_to_file_16(struct document *document, int fd)
{
	int y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);
	unsigned char color = 0;

	if (!buf) return -1;

	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			unsigned char c;
			unsigned char attr = document->data[y].chars[x].attr;
			unsigned char color1 = document->data[y].chars[x].color[0];

			if (color != color1) {
				color = color1;
				if (write_color_16(color, fd, buf, &bptr))
					goto fail;
			}

			c = document->data[y].chars[x].data;

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];

			if (c <= ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', fd, buf, &bptr))
					goto fail;
				white--;
			}

			/* Print normal char. */
			if (write_char(c, fd, buf, &bptr))
				goto fail;
		}

		/* Print end of line. */
		if (write_char('\n', fd, buf, &bptr))
			goto fail;
	}

	if (hard_write(fd, buf, bptr) != bptr) {
fail:
		mem_free(buf);
		return -1;
	}

	if (document->nlinks && get_opt_bool("document.dump.references")) {
		int x;
		unsigned char *header = "\nReferences\n\n   Visible links\n";
		int headlen = strlen(header);

		if (hard_write(fd, header, headlen) != headlen)
			goto fail;

		for (x = 0; x < document->nlinks; x++) {
			struct link *link = &document->links[x];
			unsigned char *where = link->where;

			if (!where) continue;

			if (document->options.links_numbering) {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "%4d. %s\n\t%s\n",
						 x + 1, link->title, where);
				else
					snprintf(buf, D_BUF, "%4d. %s\n",
						 x + 1, where);
			} else {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "   . %s\n\t%s\n",
						 link->title, where);
				else
					snprintf(buf, D_BUF, "   . %s\n", where);
			}

			bptr = strlen(buf);
			if (hard_write(fd, buf, bptr) != bptr)
				goto fail;
		}
	}

	mem_free(buf);
	return 0;
}

static int
write_color_256(unsigned char *str, unsigned char color, int fd, unsigned char *buf, int *bptr)
{
	unsigned char bufor[16];
	unsigned char *data = bufor;

	snprintf(bufor, 16, "\033[;%s;5;%dm", str, color);
	while(*data) {
		if (write_char(*data++, fd, buf, bptr)) return -1;
	}
	return 0;
}

static int
dump_to_file_256(struct document *document, int fd)
{
	int y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);
	unsigned char foreground = 0;
	unsigned char background = 0;

	if (!buf) return -1;

	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			unsigned char c;
			unsigned char attr = document->data[y].chars[x].attr;
			unsigned char color1 = document->data[y].chars[x].color[0];
			unsigned char color2 = document->data[y].chars[x].color[1];

			if (foreground != color1) {
				foreground = color1;
				if (write_color_256("38", foreground, fd, buf, &bptr))
					goto fail;
			}

			if (background != color2) {
				background = color2;
				if (write_color_256("48", background, fd, buf, &bptr))
					goto fail;
			}

			c = document->data[y].chars[x].data;

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];

			if (c <= ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', fd, buf, &bptr))
					goto fail;
				white--;
			}

			/* Print normal char. */
			if (write_char(c, fd, buf, &bptr))
				goto fail;
		}

		/* Print end of line. */
		if (write_char('\n', fd, buf, &bptr))
			goto fail;
	}

	if (hard_write(fd, buf, bptr) != bptr) {
fail:
		mem_free(buf);
		return -1;
	}

	if (document->nlinks && get_opt_bool("document.dump.references")) {
		int x;
		unsigned char *header = "\nReferences\n\n   Visible links\n";
		int headlen = strlen(header);

		if (hard_write(fd, header, headlen) != headlen)
			goto fail;

		for (x = 0; x < document->nlinks; x++) {
			struct link *link = &document->links[x];
			unsigned char *where = link->where;

			if (!where) continue;

			if (document->options.links_numbering) {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "%4d. %s\n\t%s\n",
						 x + 1, link->title, where);
				else
					snprintf(buf, D_BUF, "%4d. %s\n",
						 x + 1, where);
			} else {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "   . %s\n\t%s\n",
						 link->title, where);
				else
					snprintf(buf, D_BUF, "   . %s\n", where);
			}

			bptr = strlen(buf);
			if (hard_write(fd, buf, bptr) != bptr)
				goto fail;
		}
	}

	mem_free(buf);
	return 0;
}


int
dump_to_file(struct document *document, int fd)
{
	int y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);

	if (!buf) return -1;

	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

		for (x = 0; x < document->data[y].length; x++) {
			unsigned char c;
			unsigned char attr = document->data[y].chars[x].attr;

			c = document->data[y].chars[x].data;

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];

			if (c <= ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', fd, buf, &bptr))
					goto fail;
				white--;
			}

			/* Print normal char. */
			if (write_char(c, fd, buf, &bptr))
				goto fail;
		}

		/* Print end of line. */
		if (write_char('\n', fd, buf, &bptr))
			goto fail;
	}

	if (hard_write(fd, buf, bptr) != bptr) {
fail:
		mem_free(buf);
		return -1;
	}

	if (document->nlinks && get_opt_bool("document.dump.references")) {
		int x;
		unsigned char *header = "\nReferences\n\n   Visible links\n";
		int headlen = strlen(header);

		if (hard_write(fd, header, headlen) != headlen)
			goto fail;

		for (x = 0; x < document->nlinks; x++) {
			struct link *link = &document->links[x];
			unsigned char *where = link->where;

			if (!where) continue;

			if (document->options.links_numbering) {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "%4d. %s\n\t%s\n",
						 x + 1, link->title, where);
				else
					snprintf(buf, D_BUF, "%4d. %s\n",
						 x + 1, where);
			} else {
				if (link->title && *link->title)
					snprintf(buf, D_BUF, "   . %s\n\t%s\n",
						 link->title, where);
				else
					snprintf(buf, D_BUF, "   . %s\n", where);
			}

			bptr = strlen(buf);
			if (hard_write(fd, buf, bptr) != bptr)
				goto fail;
		}
	}

	mem_free(buf);
	return 0;
}

#undef D_BUF
