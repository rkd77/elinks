#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

#include "util/color.h"
struct screen_char;

/* Terminal color encoding: */
/* Below color pairs are encoded to terminal colors. Both the terminal fore-
 * and background color are a number between 0 and 7. They are stored in an
 * unsigned char as specified in the following bit sequence:
 *
 *	0bbb0fff (f = foreground, b = background)
 */

#define TERM_COLOR_MASK	0x07

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#define TERM_COLOR_FOREGROUND_256(color) ((color)[0])
#define TERM_COLOR_BACKGROUND_256(color) ((color)[1])
#endif
#define TERM_COLOR_FOREGROUND_16(color) ((color)[0] & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND_16(color) (((color)[0] >> 4) & TERM_COLOR_MASK)

/** Bit flags to control how the colors are handled. */
enum color_flags {
	/** Use a decreased color range. */
	COLOR_DECREASE_LIGHTNESS = 1,

	/** Mangle the color to stand out if attributes like underline are set.
	 * Useful for terminals that don't support these attributes.  */
	COLOR_ENHANCE_UNDERLINE = 2,

	/** Adjust the foreground color to be more readable by increasing the
	 * contrast. */
	COLOR_INCREASE_CONTRAST = 4,

	/** Adjust the contrast if the back- and foregroundcolor is equal.
	 * If inverting should be done also pass the latter flag. */
	COLOR_ENSURE_CONTRAST = 8,
	COLOR_ENSURE_INVERTED_CONTRAST = 16,
};

/** How many colors the terminal supports.
 * These numbers are used in the terminal._template_.colors and
 * document.dump.color_mode options.  They should be kept stable so
 * that configuration files are portable between ELinks versions.
 * Any unsupported modes should be treated as COLOR_MODE_16.
 * (Can't fall back to COLOR_MODE_88 from COLOR_MODE_256 because
 * the palettes are incompatible.)  */
enum color_mode {
	COLOR_MODE_DUMP = -1,
	COLOR_MODE_MONO = 0,
	COLOR_MODE_16 = 1,
#ifdef CONFIG_88_COLORS
	COLOR_MODE_88 = 2,
#endif
#ifdef CONFIG_256_COLORS
	COLOR_MODE_256 = 3,
#endif
#ifdef CONFIG_TRUE_COLOR
	COLOR_MODE_TRUE_COLOR = 4,
#endif
	COLOR_MODES = 5, /* XXX: Keep last */
};

void set_term_color16(struct screen_char *schar, enum color_flags flags,
		      unsigned char fg, unsigned char bg);

/** Mixes the color pair and attributes to a terminal text color.
 * If @a flags has masked in the ::COLOR_INCREASE_CONTRAST the
 * foreground color will be adjusted.
 *
 * XXX: @a schar may not be NULL and is modified adding stuff like
 * boldness. */

color_T get_term_color16(unsigned int index);
#ifdef CONFIG_88_COLORS
color_T get_term_color88(unsigned int index);
#endif
#ifdef CONFIG_256_COLORS
color_T get_term_color256(unsigned int index);
#endif

void get_screen_char_color(struct screen_char *schar, struct color_pair *pair,
		      enum color_flags flags, enum color_mode color_mode);
void set_term_color(struct screen_char *schar, struct color_pair *pair,
		    enum color_flags flags, enum color_mode mode);

#endif
