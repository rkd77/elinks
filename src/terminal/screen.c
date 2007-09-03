/** Terminal screen drawing routines.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/charsets.h"
#include "main/module.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: We must use termcap/terminfo if available! --pasky
 * Please mention ELinks bug 96 in commit logs.  --KON */

/** Mapping from (enum ::border_char - 0xB0) to ASCII characters.  */
const unsigned char frame_dumb[48] =	"   ||||++||++++++--|-+||++--|-+----++++++++     ";

/** Mapping from (enum ::border_char - 0xB0) to VT100 line-drawing
 * characters.  */
static const unsigned char frame_vt100[48] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

/** Mapping from (enum ::border_char - 0xB0) to VT100 line-drawing
 * characters encoded in CP437.
 * When UTF-8 I/O is enabled, ELinks uses this array instead of
 * ::frame_vt100[], and converts the characters from CP437 to UTF-8.  */
static const unsigned char frame_vt100_u[48] = {
	177, 177, 177, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 177,  32, 32,  32,  32
};

/** Mapping from (enum ::border_char - 0xB0) to obsolete FreeBSD ACS
 * graphics.
 *
 * This is for FreeBSD fonts that place graphics characters in the
 * 0x80...0x9F range, which ISO 8859 does not use.  The characters are
 * supposed to be sorted according to the codes used in VT100 or in
 * the terminfo "acsc" capability:
 *
 * @verbatim
 *	0x80	U+2588	'0'	ACS_BLOCK
 *	0x81	U+25C6	'`'	ACS_DIAMOND
 *	0x82	U+2592	'a'	ACS_CKBOARD
 *	0x83	U+2409	'b'	-
 *	0x84	U+240C	'c'	-
 *	0x85	U+240D	'd'	-
 *	0x86	U+240A	'e'	-
 *	0x87	U+00B0	'f'	ACS_DEGREE
 *	0x88	U+00B1	'g'	ACS_PLMINUS
 *	0x89	U+2424	'h'	-
 *	0x8A	U+240B	'i'	-
 *	0x8B	U+2518	'j'	ACS_LRCORNER
 *	0x8C	U+2510	'k'	ACS_URCORNER
 *	0x8D	U+250C	'l'	ACS_ULCORNER
 *	0x8E	U+2514	'm'	ACS_LLCORNER
 *	0x8F	U+253C	'n'	ACS_PLUS
 *	0x90	U+23BA	'o'	ACS_S1
 *	0x91	U+23BB	'p'	ACS_S3
 *	0x92	U+2500	'q'	ACS_HLINE
 *	0x93	U+23BC	'r'	ACS_S7
 *	0x94	U+23BD	's'	ACS_S9
 *	0x95	U+251C	't'	ACS_LTEE
 *	0x96	U+2524	'u'	ACS_RTEE
 *	0x97	U+2534	'v'	ACS_BTEE
 *	0x98	U+252C	'w'	ACS_TTEE
 *	0x99	U+2502	'x'	ACS_VLINE
 *	0x9A	U+2264	'y'	ACS_LEQUAL
 *	0x9B	U+2265	'z'	ACS_GEQUAL
 *	0x9C	U+03C0	'{'	ACS_PI
 *	0x9D	U+2260	'|'	ACS_NEQUAL
 *	0x9E	U+00A3	'}'	ACS_STERLING
 *	0x9F	U+00B7	'~'	ACS_BULLET
 * @endverbatim
 *
 * (Ncurses 5.5 defines ACS_BOARD using 'h' and ACS_LANTERN using 'i',
 * but those are not the characters meant above.)
 *
 * In FreeBSD CVS, src/share/syscons/fonts/iso-8x16.fnt revision 1.1
 * includes these characters, except it has a space at 0x80.  In
 * revision 1.2 however, all the characters not defined by ISO 8859-1
 * have been blanked out.  This change was made on 2001-11-22 and
 * included in FreeBSD 4.6.0, which was then released in June 2002.
 * Yet, support for these characters was added to Links on 2003-11-18
 * and to ELinks on 2003-12-07, so perhaps we should keep it for now.  */
static const unsigned char frame_freebsd[48] = {
	130, 138, 128, 153, 150, 150, 150, 140,
	140, 150, 153, 140, 139, 139, 139, 140,
	142, 151, 152, 149, 146, 143, 149, 149,
	142, 141, 151, 152, 149, 146, 143, 151,
	151, 152, 152, 142, 142, 141, 141, 143,
	143, 139, 141, 128, 128, 128, 128, 128,
};

/** Mapping from (enum ::border_char - 0xB0) to obsolete FreeBSD ACS
 * graphics encoded in CP437.
 * When UTF-8 I/O is enabled, ELinks uses this array instead of
 * ::frame_freebsd[], and converts the characters from CP437 to UTF-8.
 *
 * Derived from ::frame_freebsd[] by converting the characters to
 * Unicode and back to CP437.  frame_freebsd[1] = 138 = 0x8a = U+240B
 * SYMBOL FOR VERTICAL TABULATION does not exist in CP437, so we
 * substitute U+2592 MEDIUM SHADE.  */
static const unsigned char frame_freebsd_u[48] = {
	177, 177, 219, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 219, 219, 219, 219, 219,
};

/** Mapping from (enum ::border_char - 0xB0) to KOI8-R.  */
static const unsigned char frame_koi[48] = {
	144, 145, 146, 129, 135, 178, 180, 167,
	166, 181, 161, 168, 174, 173, 172, 131,
	132, 137, 136, 134, 128, 138, 175, 176,
	171, 165, 187, 184, 177, 160, 190, 185,
	186, 182, 183, 170, 169, 162, 164, 189,
	188, 133, 130, 141, 140, 142, 143, 139,
};

/** Mapping from (enum ::border_char - 0xB0) to CP850 or CP852.  Most of
 * this table is just 0xB0 + @<index in table>, because these codepages
 * are quite similar to CP437.  However, they lack some line-drawing
 * characters, so we must use others instead.  */
static const unsigned char frame_restrict[48] = {
	176, 177, 178, 179, 180, 179, 186, 186,
	205, 185, 186, 187, 188, 186, 205, 191,
	192, 193, 194, 195, 196, 197, 179, 186,
	200, 201, 202, 203, 204, 205, 206, 205,
	196, 205, 196, 186, 205, 205, 186, 186,
	179, 217, 218, 219, 220, 221, 222, 223,
};

#define TERM_STRING(str) INIT_STRING(str, sizeof(str) - 1)

/** Like add_string_to_string() but has fewer checks to slow it down.  */
#define add_term_string(str, tstr) \
	add_bytes_to_string(str, (tstr).source, (tstr).length)

/** Frame begin/end sequences that switch fonts with ECMA-48 SGR.
 * ECMA-48: CSI Ps... 06/13 = SGR - SELECT GRAPHIC RENDITION
 * - Ps = 10 = primary (default) font
 * - Ps = 11 = first alternative font */
static const struct string m11_hack_frame_seqs[] = {
	/* end border: */	TERM_STRING("\033[10m"),
	/* begin border: */	TERM_STRING("\033[11m"),
};

/** Frame begin/end sequences for VT100.  */
static const struct string vt100_frame_seqs[] = {
	/* end border: */	TERM_STRING("\x0f"),
	/* begin border: */	TERM_STRING("\x0e"),
};

/** Underline begin/end sequences using ECMA-48 SGR.
 * ECMA-48: CSI Ps... 06/13 = SGR - SELECT GRAPHIC RENDITION
 * - Ps =  4 = singly underlined
 * - Ps = 21 = doubly underlined
 * - Ps = 24 = not underlined (neither singly nor doubly) */
static const struct string underline_seqs[] = {
	/* end underline: */	TERM_STRING("\033[24m"),
	/* begin underline: */	TERM_STRING("\033[4m"),
};

/** Used in @c add_char*() and @c redraw_screen() to reduce the logic.
 * It is updated from terminal._template_.* using option.change_hook.
 *
 * @todo TODO: termcap/terminfo can maybe gradually be introduced via
 *	       this structure. We'll see. --jonas */
struct screen_driver {
	LIST_HEAD(struct screen_driver);

	/** The terminal._template_.type. Together with the #name member they
	 * uniquely identify the screen_driver. */
	enum term_mode_type type;

	/** set_screen_driver_opt() sets these.  */
	struct screen_driver_opt {
		/** Charsets when doing UTF-8 I/O.
		 * [0] is the common charset and [1] is the frame charset.
		 * Test whether to use UTF-8 I/O using the use_utf8_io() macro.  */
		int charsets[2];

		/** The frame translation table. May be NULL. */
		const unsigned char *frame;

		/** The frame mode setup and teardown sequences. May be NULL. */
		const struct string *frame_seqs;

		/** The underline mode setup and teardown sequences. May be NULL. */
		const struct string *underline;

		/** The color mode */
		enum color_mode color_mode;

		/** These are directly derived from the terminal options. */
		unsigned int transparent:1;

#ifdef CONFIG_UTF8
		/* Whether the charset of the terminal is UTF-8.  This
		 * is the same as is_cp_utf8(charsets[0]), except the
		 * latter might crash if UTF-8 I/O is disabled.  */
		unsigned int utf8_cp:1;
#endif /* CONFIG_UTF8 */
	} opt;

	/* The terminal._template_ name. */
	unsigned char name[1]; /* XXX: Keep last! */
};

/** Default options for ::TERM_DUMB.  */
static const struct screen_driver_opt dumb_screen_driver_opt = {
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_dumb,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
#ifdef CONFIG_UTF8
	/* utf8_cp: */		0,
#endif /* CONFIG_UTF8 */
};

/** Default options for ::TERM_VT100.  */
static const struct screen_driver_opt vt100_screen_driver_opt = {
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_vt100,
	/* frame_seqs: */	vt100_frame_seqs,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
#ifdef CONFIG_UTF8
	/* utf8_cp: */		0,
#endif /* CONFIG_UTF8 */
};

/** Default options for ::TERM_LINUX.  */
static const struct screen_driver_opt linux_screen_driver_opt = {
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		NULL,		/* No restrict_852 */
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
#ifdef CONFIG_UTF8
	/* utf8_cp: */		0,
#endif /* CONFIG_UTF8 */
};

/** Default options for ::TERM_KOI8.  */
static const struct screen_driver_opt koi8_screen_driver_opt = {
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_koi,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
#ifdef CONFIG_UTF8
	/* utf8_cp: */		0,
#endif /* CONFIG_UTF8 */
};

/** Default options for ::TERM_FREEBSD.  */
static const struct screen_driver_opt freebsd_screen_driver_opt = {
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_freebsd,
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
#ifdef CONFIG_UTF8
	/* utf8_cp: */		0,
#endif /* CONFIG_UTF8 */
};

/** Default options for all the different types of terminals.
 * XXX: Keep in sync with enum term_mode_type. */
static const struct screen_driver_opt *const screen_driver_opts[] = {
	/* TERM_DUMB: */	&dumb_screen_driver_opt,
	/* TERM_VT100: */	&vt100_screen_driver_opt,
	/* TERM_LINUX: */	&linux_screen_driver_opt,
	/* TERM_KOI8: */	&koi8_screen_driver_opt,
	/* TERM_FREEBSD: */	&freebsd_screen_driver_opt,
};

#define use_utf8_io(driver)	((driver)->opt.charsets[0] != -1)

static INIT_LIST_OF(struct screen_driver, active_screen_drivers);

/** Set screen_driver.opt according to screen_driver.type and @a term_spec.
 * Other members of @a *driver need not have been initialized.
 *
 * If you modify anything here, check whether option descriptions
 * should be updated.  */
static void
set_screen_driver_opt(struct screen_driver *driver, struct option *term_spec)
{
	const int cp = get_opt_codepage_tree(term_spec, "charset");
	int utf8_io = get_opt_bool_tree(term_spec, "utf_8_io");

	/* Copy all the original options from constants, so that this
	 * function need not carefully restore options one by one.  */
	copy_struct(&driver->opt, screen_driver_opts[driver->type]);

#ifdef CONFIG_UTF8
	/* Force UTF-8 I/O if the UTF-8 charset is selected.  Various
	 * places assume that the terminal's charset is unibyte if
	 * UTF-8 I/O is disabled.  (bug 827) */
	if (is_cp_utf8(cp)) {
		driver->opt.utf8_cp = 1;
		utf8_io = 1;
	} else {
		driver->opt.utf8_cp = 0;
	}
#endif /* CONFIG_UTF8 */

	driver->opt.color_mode = get_opt_int_tree(term_spec, "colors");
	driver->opt.transparent = get_opt_bool_tree(term_spec, "transparency");

	if (get_opt_bool_tree(term_spec, "underline")) {
		driver->opt.underline = underline_seqs;
	} else {
		driver->opt.underline = NULL;
	}

	if (utf8_io) {
		driver->opt.charsets[0] = cp;

		/* When we're using UTF-8 I/O, we never need to switch
		 * charsets or fonts for drawing frames, because the
		 * characters encoded in UTF-8 are already unambiguous.  */
		driver->opt.frame_seqs = NULL;

		if (driver->type == TERM_LINUX) {
			if (get_opt_bool_tree(term_spec, "restrict_852"))
				driver->opt.frame = frame_restrict;
			driver->opt.charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_FREEBSD) {
			driver->opt.frame = frame_freebsd_u;
			driver->opt.charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_VT100) {
			driver->opt.frame = frame_vt100_u;
			driver->opt.charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_KOI8) {
			driver->opt.charsets[1] = get_cp_index("koi8-r");

		} else {
#ifdef CONFIG_UTF8
			/* Don't let driver->opt.charsets[1] become
			 * UTF-8, because it is passed to cp2u(),
			 * which supports only unibyte characters.  */
			if (driver->opt.utf8_cp)
				driver->opt.charsets[1] = get_cp_index("US-ASCII");
			else
#endif	/* CONFIG_UTF8 */
				driver->opt.charsets[1] = driver->opt.charsets[0];
		}

	} else { /* !utf8_io */
		driver->opt.charsets[0] = -1;

		if (driver->type == TERM_LINUX) {
			if (get_opt_bool_tree(term_spec, "restrict_852"))
				driver->opt.frame = frame_restrict;

			if (get_opt_bool_tree(term_spec, "m11_hack"))
				driver->opt.frame_seqs = m11_hack_frame_seqs;

		} else if (driver->type == TERM_FREEBSD) {
			if (get_opt_bool_tree(term_spec, "m11_hack"))
				driver->opt.frame_seqs = m11_hack_frame_seqs;

		} else if (driver->type == TERM_VT100) {
			driver->opt.frame = frame_vt100;
		}
 	} /* !utf8_io */
}

static int
screen_driver_change_hook(struct session *ses, struct option *term_spec,
			  struct option *changed)
{
	enum term_mode_type type = get_opt_int_tree(term_spec, "type");
	struct screen_driver *driver;
	unsigned char *name = term_spec->name;

	foreach (driver, active_screen_drivers)
		if (driver->type == type && !strcmp(driver->name, name)) {
			set_screen_driver_opt(driver, term_spec);
			break;
		}

	return 0;
}

static inline struct screen_driver *
add_screen_driver(enum term_mode_type type, struct terminal *term, int env_len)
{
	struct screen_driver *driver;

	/* One byte is reserved for name in struct screen_driver. */
	driver = mem_alloc(sizeof(*driver) + env_len);
	if (!driver) return NULL;

	/* These four operations fully initialize *driver.  */
	add_to_list(active_screen_drivers, driver);
	driver->type = type;
	set_screen_driver_opt(driver, term->spec);
	memcpy(driver->name, term->spec->name, env_len + 1);

	term->spec->change_hook = screen_driver_change_hook;

#ifdef CONFIG_UTF8
	term->utf8_cp = driver->opt.utf8_cp;
	term->utf8_io = use_utf8_io(driver);
#endif /* CONFIG_UTF8 */

	return driver;
}

static inline struct screen_driver *
get_screen_driver(struct terminal *term)
{
	enum term_mode_type type = get_opt_int_tree(term->spec, "type");
	unsigned char *name = term->spec->name;
	int len = strlen(name);
	struct screen_driver *driver;

	foreach (driver, active_screen_drivers) {
		if (driver->type != type) continue;
		if (strcmp(driver->name, name)) continue;

		/* Some simple probably useless MRU ;) */
		move_to_top_of_list(active_screen_drivers, driver);

#ifdef CONFIG_UTF8
		term->utf8_cp = driver->opt.utf8_cp;
		term->utf8_io = use_utf8_io(driver);
#endif /* CONFIG_UTF8 */
		return driver;
	}

	return add_screen_driver(type, term, len);
}

/** Release private screen drawing utilities. */
void
done_screen_drivers(struct module *xxx)
{
	free_list(active_screen_drivers);
}


/** Adds the term code for positioning the cursor at @a x and @a y to
 * @a string.  The template term code is: "\033[<y>;<x>H" */
static inline struct string *
add_cursor_move_to_string(struct string *screen, int y, int x)
{
#define CURSOR_NUM_LEN 10 /* 10 chars for @y and @x numbers should be more than enough. */
	unsigned char code[4 + 2 * CURSOR_NUM_LEN + 1];
	unsigned int length = 2;

	code[0] = '\033';
	code[1] = '[';

	if (ulongcat(code, &length, y, CURSOR_NUM_LEN, 0) < 0)
		return screen;

	code[length++] = ';';

	if (ulongcat(code, &length, x, CURSOR_NUM_LEN, 0) < 0)
		return screen;

	code[length++] = 'H';

	return add_bytes_to_string(screen, code, length);
#undef CURSOR_NUM_LEN
}

struct screen_state {
	unsigned char border;
	unsigned char underline;
	unsigned char bold;
	unsigned char attr;
	/** Following should match the screen_char.color field. */
	unsigned char color[SCREEN_COLOR_SIZE];
};

#if defined(CONFIG_TRUE_COLOR)
#define INIT_SCREEN_STATE 	{ 0xFF, 0xFF, 0xFF, 0, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} }
#elif defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#define INIT_SCREEN_STATE 	{ 0xFF, 0xFF, 0xFF, 0, { 0xFF, 0xFF } }
#else
#define INIT_SCREEN_STATE 	{ 0xFF, 0xFF, 0xFF, 0, { 0xFF } }
#endif

#ifdef CONFIG_TRUE_COLOR
static inline int
compare_color_true(unsigned char *a, unsigned char *b)
{
	return !memcmp(a, b, 6);
}

static inline int
compare_bg_color_true(unsigned char *a, unsigned char *b)
{
	return (a[3] == b[3] && a[4] == b[4] && a[5] == b[5]);
}

static inline int
compare_fg_color_true(unsigned char *a, unsigned char *b)
{
	return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);
}

static inline void
copy_color_true(unsigned char *a, unsigned char *b)
{
	memcpy(a, b, 6);
}

static inline int
background_is_black(unsigned char *a)
{
	static unsigned char b[6] = {0, 0, 0, 0, 0, 0};

	return compare_bg_color_true(a, b);
}
#endif

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
static inline int
compare_color_256(unsigned char *a, unsigned char *b)
{
	return (a[0] == b[0] && a[1] == b[1]);
}

static inline int
compare_bg_color_256(unsigned char *a, unsigned char *b)
{
	return (a[1] == b[1]);
}

static inline int
compare_fg_color_256(unsigned char *a, unsigned char *b)
{
	return (a[0] == b[0]);
}

static inline void
copy_color_256(unsigned char *a, unsigned char *b)
{
	a[0] = b[0];
	a[1] = b[1];
}
#endif

static inline int
compare_color_16(unsigned char *a, unsigned char *b)
{
	return (a[0] == b[0]);
}

static inline int
compare_bg_color_16(unsigned char *a, unsigned char *b)
{
	return (TERM_COLOR_BACKGROUND_16(a) == TERM_COLOR_BACKGROUND_16(b));
}

static inline int
compare_fg_color_16(unsigned char *a, unsigned char *b)
{
	return (TERM_COLOR_FOREGROUND_16(a) == TERM_COLOR_FOREGROUND_16(b));
}

static inline void
copy_color_16(unsigned char *a, unsigned char *b)
{
	a[0] = b[0];
}

#ifdef CONFIG_UTF8
static inline void
add_char_data(struct string *screen, struct screen_driver *driver,
	      unicode_val_T data, unsigned char border)
#else  /* !CONFIG_UTF8 */
static inline void
add_char_data(struct string *screen, struct screen_driver *driver,
	      unsigned char data, unsigned char border)
#endif /* !CONFIG_UTF8 */
{
	/* charset  use_utf8_io  border  data              add_to_string
	 * -------  -----------  ------  ----------------  ----------------
	 * unibyte  0            0       terminal unibyte  terminal unibyte
	 * unibyte  0            1       enum border_char  border unibyte
	 * unibyte  1            0       terminal unibyte  UTF-8
	 * unibyte  1            1       enum border_char  UTF-8
	 * UTF-8    1            0       UTF-32 (*)        UTF-8
	 * UTF-8    1            1       enum border_char  UTF-8
	 *
	 * (*) For "UTF-32" above, data can also be UCS_NO_CHAR,
	 * in which case this function must not alter *screen.
	 */

	if (border && driver->opt.frame && data >= 176 && data < 224)
		data = driver->opt.frame[data - 176];

#ifdef CONFIG_UTF8
	if (driver->opt.utf8_cp) {
		if (border) {
			data = cp2u(driver->opt.charsets[1],
				    (unsigned char) data);
		}
		if (data == UCS_NO_CHAR)
			return;
		if (!isscreensafe_ucs(data))
			data = UCS_SPACE;
		add_to_string(screen, encode_utf8(data));
	} else
#endif /* CONFIG_UTF8 */
	if (use_utf8_io(driver)) {
		int charset = driver->opt.charsets[!!border];

		if (border || isscreensafe(data))
			add_to_string(screen, cp2utf8(charset, data));
		else /* UCS_SPACE <= 0x7F and so fits in one UTF-8 byte */
			add_char_to_string(screen, UCS_SPACE);
	} else {
		if (border || isscreensafe(data))
			add_char_to_string(screen, (unsigned char)data);
		else
			add_char_to_string(screen, ' ');
	}
}

/** Time critical section. */
static inline void
add_char16(struct string *screen, struct screen_driver *driver,
	   struct screen_char *ch, struct screen_state *state)
{
	unsigned char border = (ch->attr & SCREEN_ATTR_FRAME);
	unsigned char underline = (ch->attr & SCREEN_ATTR_UNDERLINE);
	unsigned char bold = (ch->attr & SCREEN_ATTR_BOLD);

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    border != state->border && driver->opt.frame_seqs
	   ) {
		state->border = border;
		add_term_string(screen, driver->opt.frame_seqs[!!border]);
	}

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    underline != state->underline && driver->opt.underline
	   ) {
		state->underline = underline;
		add_term_string(screen, driver->opt.underline[!!underline]);
	}

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    bold != state->bold
	   ) {
		state->bold = bold;
		if (bold) {
			add_bytes_to_string(screen, "\033[1m", 4);
		} else {
			/* Force repainting of the other attributes. */
			state->color[0] = ch->color[0] + 1;
		}
	}

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    !compare_color_16(ch->color, state->color)
	   ) {
		copy_color_16(state->color, ch->color);

		add_bytes_to_string(screen, "\033[0", 3);

		/* @set_screen_driver_opt has set @driver->opt.color_mode
		 * according to terminal-type-specific options.
		 * The caller of @add_char16 has already partially
		 * checked it, but there are still these possibilities:
		 * - COLOR_MODE_MONO.  Then don't show colors, but
		 *   perhaps use the standout attribute.
		 * - COLOR_MODE_16.  Use 16 colors.
		 * - An unsupported color mode.  Use 16 colors.  */
		if (driver->opt.color_mode != COLOR_MODE_MONO) {
			unsigned char code[6] = ";30;40";
			unsigned char bgcolor = TERM_COLOR_BACKGROUND_16(ch->color);

			code[2] += TERM_COLOR_FOREGROUND_16(ch->color);

			if (!driver->opt.transparent || bgcolor != 0) {
				code[5] += bgcolor;
				add_bytes_to_string(screen, code, 6);
			} else {
				add_bytes_to_string(screen, code, 3);
			}

		} else if (ch->attr & SCREEN_ATTR_STANDOUT) {
			/* Flip the fore- and background colors for highlighing
			 * purposes. */
			add_bytes_to_string(screen, ";7", 2);
		}

		if (underline && driver->opt.underline) {
			add_bytes_to_string(screen, ";4", 2);
		}

		/* Check if the char should be rendered bold. */
		if (bold) {
			add_bytes_to_string(screen, ";1", 2);
		}

		add_bytes_to_string(screen, "m", 1);
	}

	add_char_data(screen, driver, ch->data, border);
}

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
static const struct string color256_seqs[] = {
	/* foreground: */	TERM_STRING("\033[0;38;5;%dm"),
	/* background: */	TERM_STRING("\033[48;5;%dm"),
};

static inline void
add_char_color(struct string *screen, const struct string *seq, unsigned char color)
{
	unsigned char color_buf[3];
	unsigned char *color_pos = color_buf;
	int seq_pos = 0;
       	int color_len = 1;

	check_string_magic(seq);
	for (; seq->source[seq_pos] != '%'; seq_pos++) ;

	add_bytes_to_string(screen, seq->source, seq_pos);

	if (color < 10) {
		color_pos += 2;
	} else {
		int color2;

		++color_len;
		if (color < 100) {
			++color_pos;
		} else {
			++color_len;

			if (color < 200) {
				color_buf[0] = '1';
				color -= 100;
			} else {
				color_buf[0] = '2';
				color -= 200;
			}
		}

		color2 = (color % 10);
		color /= 10;
		color_buf[1] = '0' + color;
		color = color2;
	}

	color_buf[2] = '0' + color;

	add_bytes_to_string(screen, color_pos, color_len);

	seq_pos += 2; /* Skip "%d" */
	add_bytes_to_string(screen, &seq->source[seq_pos], seq->length - seq_pos);
}

#define add_background_color(str, seq, chr) add_char_color(str, &(seq)[1], (chr)->color[1])
#define add_foreground_color(str, seq, chr) add_char_color(str, &(seq)[0], (chr)->color[0])

/** Time critical section. */
static inline void
add_char256(struct string *screen, struct screen_driver *driver,
	    struct screen_char *ch, struct screen_state *state)
{
	unsigned char attr_delta = (ch->attr ^ state->attr);

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    attr_delta
	   ) {
		if ((attr_delta & SCREEN_ATTR_FRAME) && driver->opt.frame_seqs) {
			state->border = !!(ch->attr & SCREEN_ATTR_FRAME);
			add_term_string(screen, driver->opt.frame_seqs[state->border]);
		}

		if ((attr_delta & SCREEN_ATTR_UNDERLINE) && driver->opt.underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->opt.underline[state->underline]);
		}

		if (attr_delta & SCREEN_ATTR_BOLD) {
			if (ch->attr & SCREEN_ATTR_BOLD) {
				add_bytes_to_string(screen, "\033[1m", 4);
			} else {
				/* Force repainting of the other attributes. */
				state->color[0] = ch->color[0] + 1;
			}
		}

		state->attr = ch->attr;
	}

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    !compare_color_256(ch->color, state->color)
	   ) {
		copy_color_256(state->color, ch->color);

		add_foreground_color(screen, color256_seqs, ch);
		if (!driver->opt.transparent || ch->color[1] != 0) {
			add_background_color(screen, color256_seqs, ch);
		}

		if (ch->attr & SCREEN_ATTR_BOLD)
			add_bytes_to_string(screen, "\033[1m", 4);

		if (ch->attr & SCREEN_ATTR_UNDERLINE && driver->opt.underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->opt.underline[state->underline]);
		}
	}

	add_char_data(screen, driver, ch->data, ch->attr & SCREEN_ATTR_FRAME);
}
#endif

#ifdef CONFIG_TRUE_COLOR
static const struct string color_true_seqs[] = {
	/* foreground: */	TERM_STRING("\033[0;38;2"),
	/* background: */	TERM_STRING("\033[48;2"),
};
#define add_true_background_color(str, seq, chr) add_char_true_color(str, &(seq)[1], &(chr)->color[3])
#define add_true_foreground_color(str, seq, chr) add_char_true_color(str, &(seq)[0], &(chr)->color[0])
static inline void
add_char_true_color(struct string *screen, const struct string *seq, unsigned char *colors)
{
	unsigned char color_buf[3];
	int i;

	check_string_magic(seq);
	add_string_to_string(screen, seq);
	for (i = 0; i < 3; i++) {
		unsigned char *color_pos = color_buf;
		int color_len = 1;
		unsigned char color = colors[i];

		add_char_to_string(screen, ';');

		if (color < 10) {
			color_pos += 2;
		} else {
			int color2;

			++color_len;
			if (color < 100) {
				++color_pos;
			} else {
				++color_len;

				if (color < 200) {
					color_buf[0] = '1';
					color -= 100;
				} else {
					color_buf[0] = '2';
					color -= 200;
				}
			}

			color2 = (color % 10);
			color /= 10;
			color_buf[1] = '0' + color;
			color = color2;
		}
		color_buf[2] = '0' + color;

		add_bytes_to_string(screen, color_pos, color_len);
	}
	add_char_to_string(screen, 'm');
}

/** Time critical section. */
static inline void
add_char_true(struct string *screen, struct screen_driver *driver,
	    struct screen_char *ch, struct screen_state *state)
{
	unsigned char attr_delta = (ch->attr ^ state->attr);

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    attr_delta
	   ) {
		if ((attr_delta & SCREEN_ATTR_FRAME) && driver->opt.frame_seqs) {
			state->border = !!(ch->attr & SCREEN_ATTR_FRAME);
			add_term_string(screen, driver->opt.frame_seqs[state->border]);
		}

		if ((attr_delta & SCREEN_ATTR_UNDERLINE) && driver->opt.underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->opt.underline[state->underline]);
		}

		if (attr_delta & SCREEN_ATTR_BOLD) {
			if (ch->attr & SCREEN_ATTR_BOLD) {
				add_bytes_to_string(screen, "\033[1m", 4);
			} else {
				/* Force repainting of the other attributes. */
				state->color[0] = ch->color[0] + 1;
			}
		}

		state->attr = ch->attr;
	}

	if (
#ifdef CONFIG_UTF8
	    !(driver->opt.utf8_cp && ch->data == UCS_NO_CHAR) &&
#endif /* CONFIG_UTF8 */
	    !compare_color_true(ch->color, state->color)
	   ) {
		copy_color_true(state->color, ch->color);

		add_true_foreground_color(screen, color_true_seqs, ch);
		if (!driver->opt.transparent || !background_is_black(ch->color)) {
			add_true_background_color(screen, color_true_seqs, ch);
		}

		if (ch->attr & SCREEN_ATTR_BOLD)
			add_bytes_to_string(screen, "\033[1m", 4);

		if (ch->attr & SCREEN_ATTR_UNDERLINE && driver->opt.underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->opt.underline[state->underline]);
		}
	}

	add_char_data(screen, driver, ch->data, ch->attr & SCREEN_ATTR_FRAME);
}
#endif

#define add_chars(image_, term_, driver_, state_, ADD_CHAR, compare_bg_color, compare_fg_color)			\
{										\
	struct terminal_screen *screen = (term_)->screen;			\
	int y = screen->dirty_from;					\
	int ypos = y * (term_)->width;						\
	int prev_y = -1;							\
	int xmax = (term_)->width - 1;						\
	int ymax = (term_)->height - 1;						\
	struct screen_char *current = &screen->last_image[ypos];	\
	struct screen_char *pos = &screen->image[ypos];		\
	struct screen_char *prev_pos = NULL; /* Warning prevention. */	\
										\
	int_upper_bound(&screen->dirty_to, ymax);				\
										\
	for (; y <= screen->dirty_to; y++) {					\
		int is_last_line = (y == ymax);					\
		int x = 0;						\
										\
		for (; x <= xmax; x++, current++, pos++) {			\
			/*  Workaround for terminals without
			 *  "eat_newline_glitch (xn)", e.g., the cons25 family
			 *  of terminals and cygwin terminal.
			 *  It prevents display distortion, but char at bottom
			 *  right of terminal will not be drawn.
			 *  A better fix would be to correctly detects
			 *  terminal type, and/or add a terminal option for
			 *  this purpose. */					\
										\
			if (is_last_line && x == xmax)				\
				break;						\
										\
			if (compare_bg_color(pos->color, current->color)) {	\
				/* No update for exact match. */		\
				if (compare_fg_color(pos->color, current->color)\
				    && pos->data == current->data		\
				    && pos->attr == current->attr)		\
					continue;				\
										\
				/* Else if the color match and the data is
				 * ``space''. */				\
				if (pos->data <= ' ' && current->data <= ' '	\
				    && pos->attr == current->attr)		\
					continue;				\
			}							\
										\
			/* Move the cursor when @prev_pos is more than 10 chars
			 * away. */						\
			if (prev_y != y || prev_pos + 10 <= pos) {		\
				add_cursor_move_to_string(image_, y + 1, x + 1);\
				prev_pos = pos;					\
				prev_y = y;					\
			}							\
				 						\
			for (; prev_pos <= pos ; prev_pos++)			\
				ADD_CHAR(image_, driver_, prev_pos, state_);	\
		}								\
	}									\
}

/*! Updating of the terminal screen is done by checking what needs to
 * be updated using the last screen. */
void
redraw_screen(struct terminal *term)
{
	struct screen_driver *driver;
	struct string image;
	struct screen_state state = INIT_SCREEN_STATE;
	struct terminal_screen *screen = term->screen;

	if (!screen || screen->dirty_from > screen->dirty_to) return;
	if (term->master && is_blocked()) return;

	driver = get_screen_driver(term);
	if (!driver) return;

	if (!init_string(&image)) return;

	switch (driver->opt.color_mode) {
	default:
		/* If the desired color mode was not compiled in,
		 * use 16 colors.  */
	case COLOR_MODE_MONO:
	case COLOR_MODE_16:
		add_chars(&image, term, driver, &state, add_char16, compare_bg_color_16, compare_fg_color_16);
		break;
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
		add_chars(&image, term, driver, &state, add_char256, compare_bg_color_256, compare_fg_color_256);
		break;
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
		add_chars(&image, term, driver, &state, add_char256, compare_bg_color_256, compare_fg_color_256);
		break;
#endif
#ifdef CONFIG_TRUE_COLOR
	case COLOR_MODE_TRUE_COLOR:
		add_chars(&image, term, driver, &state, add_char_true, compare_bg_color_true, compare_fg_color_true);
		break;
#endif
	case COLOR_MODES:
	case COLOR_MODE_DUMP:
		INTERNAL("Invalid color mode (%d).", driver->opt.color_mode);
		return;
	}

	if (image.length) {
		if (driver->opt.color_mode != COLOR_MODE_MONO)
			add_bytes_to_string(&image, "\033[37;40m", 8);

		add_bytes_to_string(&image, "\033[0m", 4);

		/* If we ended in border state end the frame mode. */
		if (state.border && driver->opt.frame_seqs)
			add_term_string(&image, driver->opt.frame_seqs[0]);

	}

	/* Even if nothing was redrawn, we possibly still need to move
	 * cursor. */
	if (image.length
	    || screen->cx != screen->lcx
	    || screen->cy != screen->lcy) {
		screen->lcx = screen->cx;
		screen->lcy = screen->cy;

		add_cursor_move_to_string(&image, screen->cy + 1,
						  screen->cx + 1);
	}

	if (image.length) {
		if (term->master) want_draw();
		hard_write(term->fdout, image.source, image.length);
		if (term->master) done_draw();
	}

	done_string(&image);

	copy_screen_chars(screen->last_image, screen->image, term->width * term->height);
	screen->dirty_from = term->height;
	screen->dirty_to = 0;
}

void
erase_screen(struct terminal *term)
{
	if (term->master) {
		if (is_blocked()) return;
		want_draw();
	}

	hard_write(term->fdout, "\033[2J\033[1;1H", 10);
	if (term->master) done_draw();
}

void
beep_terminal(struct terminal *term)
{
#ifdef CONFIG_OS_WIN32
	MessageBeep(MB_ICONEXCLAMATION);
#else
	hard_write(term->fdout, "\a", 1);
#endif
}

struct terminal_screen *
init_screen(void)
{
	struct terminal_screen *screen;

	screen = mem_calloc(1, sizeof(*screen));
	if (!screen) return NULL;

	screen->lcx = -1;
	screen->lcy = -1;

	return screen;
}

/*! The two images are allocated in one chunk.
 * @todo TODO: It seems allocation failure here is fatal.
 *             We should do something! */
void
resize_screen(struct terminal *term, int width, int height)
{
	struct terminal_screen *screen;
	struct screen_char *image;
	size_t size, bsize;

	assert(term && term->screen);

	screen = term->screen;

	assert(width >= 0);
	assert(height >= 0);

	size = width * height;
	if (size <= 0) return;

	bsize = size * sizeof(*image);

	image = mem_realloc(screen->image, bsize * 2);
	if (!image) return;

	screen->image = image;
	screen->last_image = image + size;

	memset(screen->image, 0, bsize);
	memset(screen->last_image, 0xFF, bsize);

	term->width = width;
	term->height = height;
	set_screen_dirty(screen, 0, height);
}

void
done_screen(struct terminal_screen *screen)
{
	mem_free_if(screen->image);
	mem_free(screen);
}

struct module terminal_screen_module = struct_module(
	/* Because this module is a submodule of terminal_module,
	 * which is listed in main_modules rather than in builtin_modules,
	 * its name does not appear in the user interface and
	 * so need not be translatable.  */
	/* name: */		"Terminal Screen",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_screen_drivers
);
