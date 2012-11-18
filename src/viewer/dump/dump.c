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
#include "intl/charsets.h"
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

#define D_BUF	65536

#define FRAME_CHARS_BEGIN 0xB0
#define FRAME_CHARS_END   0xE0

/** A place where dumping functions write their output.  The data
 * first goes to the buffer in this structure.  When the buffer is
 * full enough, it is flushed to a file descriptor or to a string.  */
struct dump_output {
	/** How many bytes are in #buf already.  */
	size_t bufpos;

	/** A string to which the buffer should eventually be flushed,
	 * or NULL.  */
	struct string *string;

	/** A file descriptor to which the buffer should eventually be
	 * flushed, or -1.  */
	int fd;

	/** Mapping of SCREEN_ATTR_FRAME characters.  If the target
	 * codepage is UTF-8 (which is possible only if CONFIG_UTF8 is
	 * defined), then the values are UTF-32.  Otherwise, they are
	 * in the target codepage, even though the type may still be
	 * unicode_val_T.  */
#ifdef CONFIG_UTF8
	unicode_val_T frame[FRAME_CHARS_END - FRAME_CHARS_BEGIN];
#else
	unsigned char frame[FRAME_CHARS_END - FRAME_CHARS_BEGIN];
#endif

	/** Bytes waiting to be flushed.  */
	unsigned char buf[D_BUF];
};

/** Mapping from CP437 box-drawing characters to simpler CP437 characters.
 * - Map mixed light/double lines to light lines or double lines,
 *   depending on the majority; or to light lines if even.
 * - Map double lines to light lines.
 * - Map light and dark shades to medium, then to full blocks.
 * - Map half blocks to full blocks.
 * - Otherwise map to ASCII characters.  */
static const unsigned char frame_simplify[FRAME_CHARS_END - FRAME_CHARS_BEGIN]
= {
	/*-0    -1    -2    -3    -4    -5    -6    -7 */
	/*-8    -9    -A    -B    -C    -D    -E    -F */
	0xB1, 0xDB, 0xB1, '|' , '+' , 0xB4, 0xB9, 0xBF, /* 0xB0...0xB7 */
	0xC5, 0xB4, 0xB3, 0xBF, 0xD9, 0xD9, 0xD9, '+' , /* 0xB8...0xBF */
	'+' , '+' , '+' , '+' , '-' , '+' , 0xC3, 0xCC, /* 0xC0...0xC7 */
	0xC0, 0xDA, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xCA, /* 0xC8...0xCF */
	0xC1, 0xCB, 0xC2, 0xC0, 0xC0, 0xDA, 0xDA, 0xC5, /* 0xD0...0xD7 */
	0xC5, '+' , '+' , '#' , 0xDB, 0xDB, 0xDB, 0xDB  /* 0xD8...0xDF */
};

/** Initialize dump_output::frame for the specified codepage.
 *
 * If the codepage does not support all the box-drawing characters
 * of CP437, then map them to simpler characters, according to
 * frame_simplify.
 *
 * @relates dump_output */
static void
dump_output_prepare_frame(struct dump_output *out, int to_cp)
{
	const int cp437 = get_cp_index("cp437");
	int orig;
	unsigned char subst;

#ifdef CONFIG_UTF8
	if (is_cp_utf8(to_cp)) {
		for (orig = FRAME_CHARS_BEGIN; orig < FRAME_CHARS_END; orig++)
			out->frame[orig - FRAME_CHARS_BEGIN]
				= cp2u(cp437, orig);
		return;
	}
#endif /* CONFIG_UTF8 */

	for (orig = FRAME_CHARS_BEGIN; orig < FRAME_CHARS_END; orig++) {
		for (subst = orig;
		     subst >= FRAME_CHARS_BEGIN && subst < FRAME_CHARS_END;
		     subst = frame_simplify[subst - FRAME_CHARS_BEGIN]) {
			unicode_val_T ucs = cp2u(cp437, subst);
			const unsigned char *result = u2cp_no_nbsp(ucs, to_cp);

			if (result && cp2u(to_cp, result[0]) == ucs
			    && !result[1]) {
				subst = result[0];
				break;
			}
			/* Otherwise, the mapping from ucs to to_cp
			 * was not accurate, and this loop will try
			 * a simpler character.  */
		}
		out->frame[orig - FRAME_CHARS_BEGIN] = subst;
	}
}

/** Allocate and initialize a struct dump_output.
 * The caller should eventually free the structure with mem_free().
 *
 * @param fd
 *   The file descriptor to which the output will be written.
 *   Use -1 if the output should go to a string instead.
 *
 * @param string
 *   The string to which the output will be appended.
 *   Use NULL if the output should go to a file descriptor instead.
 *
 * @param cp
 *   The codepage of the dump.  It need not match the codepage
 *   of the document.
 *
 * @return The new structure, or NULL on error.
 *
 * @relates dump_output */
static struct dump_output *
dump_output_alloc(int fd, struct string *string, int cp)
{
	struct dump_output *out;

	assert((fd == -1) ^ (string == NULL));
	if_assert_failed return NULL;

	out = mem_alloc(sizeof(*out));
	if (out) {
		out->fd = fd;
		out->string = string;
		out->bufpos = 0;

		dump_output_prepare_frame(out, cp);
	}
	return out;
}

/** Flush buffered output to the file or string.
 *
 * @return 0 on success, or -1 on error.
 *
 * @post If this succeeds, then out->bufpos == 0, so that the buffer
 * has room for more data.
 *
 * @relates dump_output */
static int
dump_output_flush(struct dump_output *out)
{
	if (out->string) {
		if (!add_bytes_to_string(out->string, out->buf, out->bufpos))
			return -1;
	}
	else {
		if (hard_write(out->fd, out->buf, out->bufpos) != out->bufpos)
			return -1;
	}

	out->bufpos = 0;
	return 0;
}

static int
write_char(unsigned char c, struct dump_output *out)
{
	if (out->bufpos >= D_BUF) {
		if (dump_output_flush(out))
			return -1;
	}

	out->buf[out->bufpos++] = c;
	return 0;
}

static int
write_color_16(unsigned char color, struct dump_output *out)
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
		if (write_char(*data++, out)) return -1;
	}
	return 0;
}

#define DUMP_COLOR_MODE_16
#define DUMP_FUNCTION_COLOR   dump_16color
#define DUMP_FUNCTION_UTF8    dump_16color_utf8
#define DUMP_FUNCTION_UNIBYTE dump_16color_unibyte
#include "dump-color-mode.h"
#undef DUMP_COLOR_MODE_16
#undef DUMP_FUNCTION_COLOR
#undef DUMP_FUNCTION_UTF8
#undef DUMP_FUNCTION_UNIBYTE

/* configure --enable-debug uses gcc -Wall -Werror, and -Wall includes
 * -Wunused-function, so declaring or defining any unused function
 * would break the build. */
#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)

static int
write_color_256(const unsigned char *str, unsigned char color,
		struct dump_output *out)
{
	unsigned char bufor[16];
	unsigned char *data = bufor;

	snprintf(bufor, 16, "\033[%s;5;%dm", str, color);
	while(*data) {
		if (write_char(*data++, out)) return -1;
	}
	return 0;
}

#define DUMP_COLOR_MODE_256
#define DUMP_FUNCTION_COLOR   dump_256color
#define DUMP_FUNCTION_UTF8    dump_256color_utf8
#define DUMP_FUNCTION_UNIBYTE dump_256color_unibyte
#include "dump-color-mode.h"
#undef DUMP_COLOR_MODE_256
#undef DUMP_FUNCTION_COLOR
#undef DUMP_FUNCTION_UTF8
#undef DUMP_FUNCTION_UNIBYTE

#endif /* defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS) */

#ifdef CONFIG_TRUE_COLOR

static int
write_true_color(const unsigned char *str, const unsigned char *color,
		 struct dump_output *out)
{
	unsigned char bufor[24];
	unsigned char *data = bufor;

	snprintf(bufor, 24, "\033[%s;2;%d;%d;%dm", str, color[0], color[1], color[2]);
	while(*data) {
		if (write_char(*data++, out)) return -1;
	}
	return 0;
}

#define DUMP_COLOR_MODE_TRUE
#define DUMP_FUNCTION_COLOR   dump_truecolor
#define DUMP_FUNCTION_UTF8    dump_truecolor_utf8
#define DUMP_FUNCTION_UNIBYTE dump_truecolor_unibyte
#include "dump-color-mode.h"
#undef DUMP_COLOR_MODE_TRUE
#undef DUMP_FUNCTION_COLOR
#undef DUMP_FUNCTION_UTF8
#undef DUMP_FUNCTION_UNIBYTE

#endif /* CONFIG_TRUE_COLOR */

#define DUMP_COLOR_MODE_NONE
#define DUMP_FUNCTION_COLOR   dump_nocolor
#define DUMP_FUNCTION_UTF8    dump_nocolor_utf8
#define DUMP_FUNCTION_UNIBYTE dump_nocolor_unibyte
#include "dump-color-mode.h"
#undef DUMP_COLOR_MODE_NONE
#undef DUMP_FUNCTION_COLOR
#undef DUMP_FUNCTION_UTF8
#undef DUMP_FUNCTION_UNIBYTE

/*! @return 0 on success, -1 on error */
static int
dump_references(struct document *document, int fd, unsigned char buf[D_BUF])
{
	if (document->nlinks
	    && get_opt_bool("document.dump.references", NULL)) {
		int x;
		unsigned char *header = "\nReferences\n\n   Visible links\n";
		int headlen = strlen(header);

		if (hard_write(fd, header, headlen) != headlen)
			return -1;

		for (x = 0; x < document->nlinks; x++) {
			struct link *link = &document->links[x];
			unsigned char *where = link->where;
			size_t reflen;

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

			reflen = strlen(buf);
			if (hard_write(fd, buf, reflen) != reflen)
				return -1;
		}
	}

	return 0;
}

int
dump_to_file(struct document *document, int fd)
{
	struct dump_output *out = dump_output_alloc(fd, NULL,
						    document->options.cp);
	int error;

	if (!out) return -1;

	error = dump_nocolor(document, out);
	if (!error)
		error = dump_references(document, fd, out->buf);

	mem_free(out);
	return error;
}

/* This dumps the given @cached's formatted output onto @fd. */
static void
dump_formatted(int fd, struct download *download, struct cache_entry *cached)
{
	struct document_options o;
	struct document_view formatted;
	struct view_state vs;
	int width;
	struct dump_output *out;

	if (!cached) return;

	memset(&formatted, 0, sizeof(formatted));

	init_document_options(NULL, &o);
	width = get_opt_int("document.dump.width", NULL);
	set_box(&o.box, 0, 1, width, DEFAULT_TERMINAL_HEIGHT);

	o.cp = get_opt_codepage("document.dump.codepage", NULL);
	o.color_mode = get_opt_int("document.dump.color_mode", NULL);
	o.plain = 0;
	o.frames = 0;
	o.links_numbering = get_opt_bool("document.dump.numbering", NULL);

	init_vs(&vs, cached->uri, -1);

	render_document(&vs, &formatted, &o);

	out = dump_output_alloc(fd, NULL, o.cp);
	if (out) {
		int error;

		switch (o.color_mode) {
		case COLOR_MODE_DUMP:
		case COLOR_MODE_MONO: /* FIXME: inversion */
			error = dump_nocolor(formatted.document, out);
			break;

		default:
			/* If the desired color mode was not compiled in,
			 * use 16 colors.  */
		case COLOR_MODE_16:
			error = dump_16color(formatted.document, out);
			break;

#ifdef CONFIG_88_COLORS
		case COLOR_MODE_88:
			error = dump_256color(formatted.document, out);
			break;
#endif

#ifdef CONFIG_256_COLORS
		case COLOR_MODE_256:
			error = dump_256color(formatted.document, out);
			break;
#endif

#ifdef CONFIG_TRUE_COLOR
		case COLOR_MODE_TRUE_COLOR:
			error = dump_truecolor(formatted.document, out);
			break;
#endif
		}

		if (!error)
			dump_references(formatted.document, fd, out->buf);

		mem_free(out);
	} /* if out */

	detach_formatted(&formatted);
	destroy_vs(&vs, 1);
}

#undef D_BUF

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
	unsigned char *str = get_opt_str(option, NULL);

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

	if (!is_in_state(download->state, S_OK)) {
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
dump_next(LIST_OF(struct string_list_item) *url_list)
{
	static INIT_LIST_OF(struct string_list_item, todo_list);
	static INIT_LIST_OF(struct string_list_item, done_list);
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

struct string *
add_document_to_string(struct string *string, struct document *document)
{
	struct dump_output *out;
	int error;

	assert(string && document);
	if_assert_failed return NULL;

	out = dump_output_alloc(-1, string, document->options.cp);
	if (!out) return NULL;

	error = dump_nocolor(document, out);

	mem_free(out);
	return error ? NULL : string;
}
