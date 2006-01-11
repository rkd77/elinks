/* Terminal screen drawing routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/charsets.h"
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


/* TODO: We must use termcap/terminfo if available! --pasky */

unsigned char frame_dumb[48] =	"   ||||++||++++++--|-+||++--|-+----++++++++     ";
static unsigned char frame_vt100[48] =	"aaaxuuukkuxkjjjkmvwtqnttmlvwtqnvvwwmmllnnjla    ";

/* For UTF8 I/O */
static unsigned char frame_vt100_u[48] = {
	177, 177, 177, 179, 180, 180, 180, 191,
	191, 180, 179, 191, 217, 217, 217, 191,
	192, 193, 194, 195, 196, 197, 195, 195,
	192, 218, 193, 194, 195, 196, 197, 193,
	193, 194, 194, 192, 192, 218, 218, 197,
	197, 217, 218, 177,  32, 32,  32,  32
};

static unsigned char frame_freebsd[48] = {
	130, 138, 128, 153, 150, 150, 150, 140,
	140, 150, 153, 140, 139, 139, 139, 140,
	142, 151, 152, 149, 146, 143, 149, 149,
	142, 141, 151, 152, 149, 146, 143, 151,
	151, 152, 152, 142, 142, 141, 141, 143,
	143, 139, 141, 128, 128, 128, 128, 128,
};

static unsigned char frame_koi[48] = {
	144, 145, 146, 129, 135, 178, 180, 167,
	166, 181, 161, 168, 174, 173, 172, 131,
	132, 137, 136, 134, 128, 138, 175, 176,
	171, 165, 187, 184, 177, 160, 190, 185,
	186, 182, 183, 170, 169, 162, 164, 189,
	188, 133, 130, 141, 140, 142, 143, 139,
};

/* Most of this table is just 176 + <index in table>. */
static unsigned char frame_restrict[48] = {
	176, 177, 178, 179, 180, 179, 186, 186,
	205, 185, 186, 187, 188, 186, 205, 191,
	192, 193, 194, 195, 196, 197, 179, 186,
	200, 201, 202, 203, 204, 205, 206, 205,
	196, 205, 196, 186, 205, 205, 186, 186,
	179, 217, 218, 219, 220, 221, 222, 223,
};

#define TERM_STRING(str) INIT_STRING(str, sizeof(str) - 1)

#define add_term_string(str, tstr) \
	add_bytes_to_string(str, (tstr).source, (tstr).length)

static struct string m11_hack_frame_seqs[] = {
	/* end border: */	TERM_STRING("\033[10m"),
	/* begin border: */	TERM_STRING("\033[11m"),
};

static struct string vt100_frame_seqs[] = {
	/* end border: */	TERM_STRING("\x0f"),
	/* begin border: */	TERM_STRING("\x0e"),
};

static struct string underline_seqs[] = {
	/* begin underline: */	TERM_STRING("\033[24m"),
	/* end underline: */	TERM_STRING("\033[4m"),
};

/* Used in {add_char*()} and {redraw_screen()} to reduce the logic. It is
 * updated from terminal._template_.* using option change_hooks. */
/* TODO: termcap/terminfo can maybe gradually be introduced via this
 *	 structure. We'll see. --jonas */
struct screen_driver {
	LIST_HEAD(struct screen_driver);

	/* The terminal._template_.type. Together with the @name member the
	 * uniquely identify the screen_driver. */
	enum term_mode_type type;

	/* Charsets when doing UTF8 I/O. */
	/* [0] is the common charset and [1] is the frame charset.
	 * Test wether to use UTF8 I/O using the use_utf8_io() macro. */
	int charsets[2];

	/* The frame translation table. May be NULL. */
	unsigned char *frame;

	/* The frame mode setup and teardown sequences. May be NULL. */
	struct string *frame_seqs;

	/* The underline mode setup and teardown sequences. May be NULL. */
	struct string *underline;

	/* The color mode */
	enum color_mode color_mode;

	/* These are directly derived from the terminal options. */
	unsigned int transparent:1;

	/* The terminal._template_ name. */
	unsigned char name[1]; /* XXX: Keep last! */
};

static struct screen_driver dumb_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_DUMB,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_dumb,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
};

static struct screen_driver vt100_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_VT100,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_vt100,
	/* frame_seqs: */	vt100_frame_seqs,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
};

static struct screen_driver linux_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_LINUX,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		NULL,		/* No restrict_852 */
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
};

static struct screen_driver koi8_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_KOI8,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_koi,
	/* frame_seqs: */	NULL,
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
};

static struct screen_driver freebsd_screen_driver = {
				NULL_LIST_HEAD,
	/* type: */		TERM_FREEBSD,
	/* charsets: */		{ -1, -1 },	/* No UTF8 I/O */
	/* frame: */		frame_freebsd,
	/* frame_seqs: */	NULL,		/* No m11_hack */
	/* underline: */	underline_seqs,
	/* color_mode: */	COLOR_MODE_16,
	/* transparent: */	1,
};

/* XXX: Keep in sync with enum term_mode_type. */
static struct screen_driver *screen_drivers[] = {
	/* TERM_DUMB: */	&dumb_screen_driver,
	/* TERM_VT100: */	&vt100_screen_driver,
	/* TERM_LINUX: */	&linux_screen_driver,
	/* TERM_KOI8: */	&koi8_screen_driver,
	/* TERM_FREEBSD: */	&freebsd_screen_driver,
};


static INIT_LIST_HEAD(active_screen_drivers);

static void
update_screen_driver(struct screen_driver *driver, struct option *term_spec)
{
	int utf8_io = get_opt_bool_tree(term_spec, "utf_8_io");

	driver->color_mode = get_opt_int_tree(term_spec, "colors");
	driver->transparent = get_opt_bool_tree(term_spec, "transparency");

	if (get_opt_bool_tree(term_spec, "underline")) {
		driver->underline = underline_seqs;
	} else {
		driver->underline = NULL;
	}

	if (utf8_io) {
		driver->charsets[0] = get_opt_codepage_tree(term_spec, "charset");
		if (driver->type == TERM_LINUX) {
			if (get_opt_bool_tree(term_spec, "restrict_852"))
				driver->frame = frame_restrict;

			driver->charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_FREEBSD) {
			driver->charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_VT100) {
			driver->frame = frame_vt100_u;
			driver->charsets[1] = get_cp_index("cp437");

		} else if (driver->type == TERM_KOI8) {
			driver->charsets[1] = get_cp_index("koi8-r");

		} else {
			driver->charsets[1] = driver->charsets[0];
		}

	} else {
		driver->charsets[0] = -1;
		if (driver->type == TERM_LINUX) {
			if (get_opt_bool_tree(term_spec, "restrict_852"))
				driver->frame = frame_restrict;

			if (get_opt_bool_tree(term_spec, "m11_hack"))
				driver->frame_seqs = m11_hack_frame_seqs;

		} else if (driver->type == TERM_FREEBSD) {
			if (get_opt_bool_tree(term_spec, "m11_hack"))
				driver->frame_seqs = m11_hack_frame_seqs;

		} else if (driver->type == TERM_VT100) {
			driver->frame = frame_vt100;
		}
	}
}

static int
screen_driver_change_hook(struct session *ses, struct option *term_spec,
			  struct option *changed)
{
	enum term_mode_type type = get_opt_int_tree(term_spec, "type");
	struct screen_driver *driver;
	unsigned char *name = term_spec->name;
	int len = strlen(name);

	foreach (driver, active_screen_drivers)
		if (driver->type == type && !memcmp(driver->name, name, len)) {
			update_screen_driver(driver, term_spec);
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

	memcpy(driver, screen_drivers[type], sizeof(*driver) - 1);
	memcpy(driver->name, term->spec->name, env_len + 1);

	add_to_list(active_screen_drivers, driver);

	update_screen_driver(driver, term->spec);

	term->spec->change_hook = screen_driver_change_hook;

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
		if (memcmp(driver->name, name, len + 1)) continue;

		/* Some simple probably useless MRU ;) */
		move_to_top_of_list(active_screen_drivers, driver);

		return driver;
	}

	return add_screen_driver(type, term, len);
}

void
done_screen_drivers(void)
{
	free_list(active_screen_drivers);
}


/* Adds the term code for positioning the cursor at @x and @y to @string.
 * The template term code is: "\033[<@y>;<@x>H" */
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
	/* Following should match struct screen_char color field. */
#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
	unsigned char color[2];
#else
	unsigned char color[1];
#endif
};

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#define compare_color(a, b)	((a)[0] == (b)[0] && (a)[1] == (b)[1])
#define copy_color(a, b)	do { (a)[0] = (b)[0]; (a)[1] = (b)[1]; } while (0)
#define INIT_SCREEN_STATE 	{ 0xFF, 0xFF, 0xFF, 0, { 0xFF, 0xFF } }
#else
#define compare_color(a, b)	((a)[0] == (b)[0])
#define copy_color(a, b)	do { (a)[0] = (b)[0]; } while (0)
#define INIT_SCREEN_STATE 	{ 0xFF, 0xFF, 0xFF, 0, { 0xFF } }
#endif

#define compare_bg_color(a, b)	(TERM_COLOR_BACKGROUND(a) == TERM_COLOR_BACKGROUND(b))
#define compare_fg_color(a, b)	(TERM_COLOR_FOREGROUND(a) == TERM_COLOR_FOREGROUND(b))

#define use_utf8_io(driver)	((driver)->charsets[0] != -1)

static inline void
add_char_data(struct string *screen, struct screen_driver *driver,
	      unsigned char data, unsigned char border)
{
	if (!isscreensafe(data)) {
		add_char_to_string(screen, ' ');
		return;
	}

	if (border && driver->frame && data >= 176 && data < 224)
		data = driver->frame[data - 176];

	if (use_utf8_io(driver)) {
		int charset = driver->charsets[!!border];

		add_to_string(screen, cp2utf_8(charset, data));
		return;
	}

	add_char_to_string(screen, data);
}

/* Time critical section. */
static inline void
add_char16(struct string *screen, struct screen_driver *driver,
	   struct screen_char *ch, struct screen_state *state)
{
	unsigned char border = (ch->attr & SCREEN_ATTR_FRAME);
	unsigned char underline = (ch->attr & SCREEN_ATTR_UNDERLINE);
	unsigned char bold = (ch->attr & SCREEN_ATTR_BOLD);

	if (border != state->border && driver->frame_seqs) {
		state->border = border;
		add_term_string(screen, driver->frame_seqs[!!border]);
	}

	if (underline != state->underline && driver->underline) {
		state->underline = underline;
		add_term_string(screen, driver->underline[!!underline]);
	}

	if (bold != state->bold) {
		state->bold = bold;
		if (bold) {
			add_bytes_to_string(screen, "\033[1m", 4);
		} else {
			/* Force repainting of the other attributes. */
			state->color[0] = ch->color[0] + 1;
		}
	}

	if (!compare_color(ch->color, state->color)) {
		copy_color(state->color, ch->color);

		add_bytes_to_string(screen, "\033[0", 3);

		if (driver->color_mode == COLOR_MODE_16) {
			unsigned char code[6] = ";30;40";
			unsigned char bgcolor = TERM_COLOR_BACKGROUND(ch->color);

			code[2] += TERM_COLOR_FOREGROUND(ch->color);

			if (!driver->transparent || bgcolor != 0) {
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

		if (underline && driver->underline) {
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
static struct string color256_seqs[] = {
	/* foreground: */	TERM_STRING("\033[0;38;5;%dm"),
	/* background: */	TERM_STRING("\033[48;5;%dm"),
};

static inline void
add_char_color(struct string *screen, struct string *seq, unsigned char color)
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

/* Time critical section. */
static inline void
add_char256(struct string *screen, struct screen_driver *driver,
	    struct screen_char *ch, struct screen_state *state)
{
	unsigned char attr_delta = (ch->attr ^ state->attr);

	if (attr_delta) {
		if ((attr_delta & SCREEN_ATTR_FRAME) && driver->frame_seqs) {
			state->border = !!(ch->attr & SCREEN_ATTR_FRAME);
			add_term_string(screen, driver->frame_seqs[state->border]);
		}

		if ((attr_delta & SCREEN_ATTR_UNDERLINE) && driver->underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->underline[state->underline]);
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

	if (!compare_color(ch->color, state->color)) {
		copy_color(state->color, ch->color);

		add_foreground_color(screen, color256_seqs, ch);
		if (!driver->transparent || ch->color[1] != 0) {
			add_background_color(screen, color256_seqs, ch);
		}

		if (ch->attr & SCREEN_ATTR_BOLD)
			add_bytes_to_string(screen, "\033[1m", 4);

		if (ch->attr & SCREEN_ATTR_UNDERLINE && driver->underline) {
			state->underline = !!(ch->attr & SCREEN_ATTR_UNDERLINE);
			add_term_string(screen, driver->underline[state->underline]);
		}
	}

	add_char_data(screen, driver, ch->data, ch->attr & SCREEN_ATTR_FRAME);
}
#endif

#define add_chars(image_, term_, driver_, state_, ADD_CHAR)			\
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

/* Updating of the terminal screen is done by checking what needs to be updated
 * using the last screen. */
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

	switch (driver->color_mode) {
	case COLOR_MODE_MONO:
	case COLOR_MODE_16:
		add_chars(&image, term, driver, &state, add_char16);
		break;
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
		add_chars(&image, term, driver, &state, add_char256);
		break;
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
		add_chars(&image, term, driver, &state, add_char256);
		break;
#endif
	case COLOR_MODES:
	case COLOR_MODE_DUMP:
	default:
		INTERNAL("Invalid color mode (%d).", driver->color_mode);
		return;
	}

	if (image.length) {
		if (driver->color_mode)
			add_bytes_to_string(&image, "\033[37;40m", 8);

		add_bytes_to_string(&image, "\033[0m", 4);

		/* If we ended in border state end the frame mode. */
		if (state.border && driver->frame_seqs)
			add_term_string(&image, driver->frame_seqs[0]);

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

/* The two images are allocated in one chunk. */
/* TODO: It seems allocation failure here is fatal. We should do something! */
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
