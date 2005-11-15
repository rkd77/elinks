#ifndef EL__TERMINAL_COLOR_H
#define EL__TERMINAL_COLOR_H

struct color_pair;
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
#define TERM_COLOR_FOREGROUND(color) ((color)[0])
#define TERM_COLOR_BACKGROUND(color) ((color)[1])
#else
#define TERM_COLOR_FOREGROUND(color) ((color)[0] & TERM_COLOR_MASK)
#define TERM_COLOR_BACKGROUND(color) (((color)[0] >> 4) & TERM_COLOR_MASK)
#endif

/* Bit flags to control how the colors are handled. */
enum color_flags {
	/* Use a decreased color range. */
	COLOR_DECREASE_LIGHTNESS = 1,

	/* Mangle the color to stand out if attributes like underline are set.
	 * Useful for terminals that doesn't support these attributes.  */
	COLOR_ENHANCE_UNDERLINE = 2,

	/* Adjust the forground color to be more readable by increasing the
	 * contrast. */
	COLOR_INCREASE_CONTRAST = 4,

	/* Adjust the contrast if the back- and foregroundcolor is equal.
	 * If inverting should be done also pass the latter flag. */
	COLOR_ENSURE_CONTRAST = 8,
	COLOR_ENSURE_INVERTED_CONTRAST = 16,
};

enum color_mode {
	COLOR_MODE_DUMP = -1,
	COLOR_MODE_MONO,
	COLOR_MODE_16,
#ifdef CONFIG_88_COLORS
	COLOR_MODE_88,
#endif
#ifdef CONFIG_256_COLORS
	COLOR_MODE_256,
#endif

	COLOR_MODES, /* XXX: Keep last */
};

/* Mixes the color pair and attributes to a terminal text color. */
/* If @flags has masked in the COLOR_INCREASE_CONTRAST the foreground color will
 * be adjusted. */
/* XXX: @schar may not be NULL and is modified adding stuff like boldness. */
void set_term_color(struct screen_char *schar, struct color_pair *pair,
		    enum color_flags flags, enum color_mode mode);

#endif
