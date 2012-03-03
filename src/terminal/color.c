/** Terminal color composing.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/error.h"

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

#include "terminal/palette.inc"

struct rgb_cache_entry {
	int color;
	int level;
	color_T rgb;
};

static inline int
color_distance(const struct rgb *c1, const struct rgb *c2)
{
	int r = c1->r - c2->r;
	int g = c1->g - c2->g;
	int b = c1->b - c2->b;

	return (3 * r * r) + (4 * g * g) + (2 * b * b);
}

#define RED_COLOR_MASK		0x00FF0000
#define GREEN_COLOR_MASK	0x0000FF00
#define BLUE_COLOR_MASK		0x000000FF

#define RED_COLOR(color)	(((color) & RED_COLOR_MASK)   >> 16)
#define GREEN_COLOR(color)	(((color) & GREEN_COLOR_MASK) >>  8)
#define BLUE_COLOR(color)	(((color) & BLUE_COLOR_MASK)  >>  0)

#define RED(color)	(RED_COLOR(color)   << 3)
#define GREEN(color)	(GREEN_COLOR(color) << 2)
#define BLUE(color)	(BLUE_COLOR(color)  << 0)

#define RGBCOLOR(color)	(RED(color) + GREEN(color) + BLUE(color))

#define RGB_HASH_SIZE		4096
#define HASH_RGB(color, l)	((RGBCOLOR(color) + (l)) & (RGB_HASH_SIZE - 1))

/** Initialize a rgb struct from a color_T
 * @relates rgb */
#define INIT_RGB(color) \
	{ RED_COLOR(color), GREEN_COLOR(color), BLUE_COLOR(color) }

/** Locates the nearest terminal color. */
static inline unsigned char
get_color(color_T color, const struct rgb *palette, int level)
{
	static struct rgb_cache_entry cache[RGB_HASH_SIZE];
	struct rgb_cache_entry *rgb_cache = &cache[HASH_RGB(color, level)];

	/* Uninitialized cache entries have level set to zero. */
	if (rgb_cache->level == 0
	    || rgb_cache->level != level
	    || rgb_cache->rgb != color) {
		struct rgb rgb = INIT_RGB(color);
		unsigned char nearest_color = 0;
		int min_dist = color_distance(&rgb, &palette[0]);
		int i;

#ifdef CONFIG_DEBUG
		if (level <= 0)
			INTERNAL("get_color() called with @level <= 0");
#endif

		for (i = 1; i < level; i++) {
			int dist = color_distance(&rgb, &palette[i]);

			if (dist < min_dist) {
				min_dist = dist;
				nearest_color = i;
			}
		}

		rgb_cache->color = nearest_color;
		rgb_cache->level = level;
		rgb_cache->rgb = color;
	}

	return rgb_cache->color;
}

#undef INIT_RGB
#undef HASH_RGB
#undef RGB_HASH_SIZE

#undef RED
#undef GREEN
#undef BLUE

#undef RED_COLOR
#undef GREEN_COLOR
#undef BLUE_COLOR

#undef RED_COLOR_MASK
#undef GREEN_COLOR_MASK
#undef BLUE_COLOR_MASK

/** Controls what color ranges to use when setting the terminal color.
 * @todo TODO: Part of the 256 color palette is gray scale, maybe we
 * could experiment with a grayscale mode. ;) --jonas */
enum palette_range {
       PALETTE_FULL = 0,
       PALETTE_HALF,

       PALETTE_RANGES, /* XXX: Keep last */
};

struct color_mode_info {
	const struct rgb *palette;

	struct {
		int bg;
		int fg;
	} palette_range[PALETTE_RANGES];
};

static const struct color_mode_info color_mode_16 = {
	palette16,
	{
		/* PALETTE_FULL */	{ 8, 16 },
		/* PALETTE_HALF */	{ 8,  8 },
	}
};

#ifdef CONFIG_88_COLORS
static const struct color_mode_info color_mode_88 = {
	palette88,
	{
		/* PALETTE_FULL */	{ 88, 88 },
		/* PALETTE_HALF */	{ 88, 44 },
	}
};
#endif

#ifdef CONFIG_256_COLORS
static const struct color_mode_info color_mode_256 = {
	palette256,
	{
		/* PALETTE_FULL */	{ 256, 256 },
		/* PALETTE_HALF */	{ 256, 128 },
	}
};
#endif

static const struct color_mode_info *const color_modes[] = {
	/* COLOR_MODE_MONO */	&color_mode_16,
	/* COLOR_MODE_16 */	&color_mode_16,
#ifdef CONFIG_88_COLORS
	/* COLOR_MODE_88 */	&color_mode_88,
#else
	/* COLOR_MODE_88 */	&color_mode_16,
#endif
#ifdef CONFIG_256_COLORS
	/* COLOR_MODE_256 */	&color_mode_256,
#else
	/* COLOR_MODE_256 */	&color_mode_16,
#endif
	/* @set_term_color reads @color_modes[COLOR_MODE_TRUE_COLOR]
	 * only if CONFIG_TRUE_COLOR is not defined.  */
	/* COLOR_MODE_TRUE_COLOR */ &color_mode_16,
};
/** Get a compile-time error if the ::color_modes array has the wrong
 * size.  */
typedef int assert_enough_color_modes[
	(sizeof(color_modes) / sizeof(color_modes[0]) == COLOR_MODES)
	? 1 : -1];

/* Colors values used in the foreground color table:
 *
 *	0 == black	 8 == darkgrey (brightblack ;)
 *	1 == red	 9 == brightred
 *	2 == green	10 == brightgreen
 *	3 == brown	11 == brightyellow
 *	4 == blue	12 == brightblue
 *	5 == magenta	13 == brightmagenta
 *	6 == cyan	14 == brightcyan
 *	7 == white	15 == brightwhite
 *
 * Bright colors will be rendered bold. */

/** Map foreground colors to more visible ones on various backgrounds.
 * Use like: fg = fg_color[fg][bg];
 *
 * This table is based mostly on wild guesses of mine. Feel free to
 * correct it. --pasky */
static const unsigned char fg_color[16][8] = {
	/* bk  r  gr  br  bl   m   c   w */

	/* 0 (black) */
	{  7,  0,  0,  0,  7,  0,  0,  0 },
	/* 1 (red) */
	{  1,  9,  1,  9,  9,  9,  1,  1 },
	/* 2 (green) */
	{  2,  2, 10,  2,  2,  2, 10, 10 },
	/* 3 (brown) */
	{  3, 11,  3, 11,  3, 11,  3,  3 },
	/* 4 (blue) */
	{ 12, 12,  6,  4, 12,  6,  4,  4 },
	/* 5 (magenta) */
	{  5, 13,  5, 13, 13, 13,  5,  5 },
	/* 6 (cyan) */
	{  6,  6, 14,  6,  6,  6, 14, 14 },
	/* 7 (grey) */
	{  7,  7,  0,  7,  7,  7,  0,  0 }, /* Don't s/0/8/, messy --pasky */
	/* 8 (darkgrey) */
	{ 15, 15,  8, 15, 15, 15,  8,  8 },
	/* 9 (brightred) */
	{  9,  9,  1,  9,  9,  9,  1,  9 }, /* I insist on 7->9 --pasky */
	/* 10 (brightgreen) */
	{ 10, 10, 10, 10, 10, 10, 10, 10 },
	/* 11 (brightyellow) */
	{ 11, 11, 11, 11, 11, 11, 11, 11 },
	/* 12 (brightblue) */
	{ 12, 12,  6,  4,  6,  6,  4, 12 },
	/* 13 (brightmagenta) */
	{ 13, 13,  5, 13, 13, 13,  5,  5 },
	/* 14 (brightcyan) */
	{ 14, 14, 14, 14, 14, 14, 14, 14 },
	/* 15 (brightwhite) */
	{ 15, 15, 15, 15, 15, 15, 15, 15 },
};

/* When determining whether to use negative image we make the most significant
 * be least significant. */
#define CMPCODE(c) (((c) << 1 | (c) >> 2) & TERM_COLOR_MASK)
#define use_inverse(bg, fg) CMPCODE(fg & TERM_COLOR_MASK) < CMPCODE(bg)

NONSTATIC_INLINE void
set_term_color16(struct screen_char *schar, enum color_flags flags,
		 unsigned char fg, unsigned char bg)
{
	/* Adjusts the foreground color to be more visible. */
	if (flags & COLOR_INCREASE_CONTRAST) {
		fg = fg_color[fg][bg];
	}

	/* Add various color enhancement based on the attributes. */
	if (schar->attr) {
		if (schar->attr & SCREEN_ATTR_ITALIC)
			fg ^= 0x01;

		if (schar->attr & SCREEN_ATTR_BOLD)
			fg |= SCREEN_ATTR_BOLD;

		if ((schar->attr & SCREEN_ATTR_UNDERLINE)
		    && (flags & COLOR_ENHANCE_UNDERLINE)) {
			fg |= SCREEN_ATTR_BOLD;
			fg ^= 0x04;
		}
	}

	/* Adjusts the foreground color to be more visible. */
	if ((flags & COLOR_INCREASE_CONTRAST)
	    || (bg == fg && (flags & COLOR_ENSURE_CONTRAST))) {
		if (flags & COLOR_ENSURE_INVERTED_CONTRAST) {
			unsigned char contrastbg = fg_color[fg][bg];

			fg = bg;
			bg = contrastbg;
		} else {
			fg = fg_color[fg][bg];
		}
	}

	if (fg & SCREEN_ATTR_BOLD) {
		schar->attr |= SCREEN_ATTR_BOLD;
	}

	if (use_inverse(bg, fg)) {
		schar->attr |= SCREEN_ATTR_STANDOUT;
	}
	schar->c.color[0] = (bg << 4 | fg);
}

color_T
get_term_color16(unsigned int index)
{
	if (index > 15) return 0;
	return ((palette16[index].r << 16) |
		(palette16[index].g << 8) |
		palette16[index].b);
}

#ifdef CONFIG_88_COLORS
color_T
get_term_color88(unsigned int index)
{
	if (index > 87) return 0;
	return ((palette88[index].r << 16) |
		(palette88[index].g << 8) |
		palette88[index].b);
}
#endif

#ifdef CONFIG_256_COLORS
color_T
get_term_color256(unsigned int index)
{
	if (index > 255) return 0;
	return ((palette256[index].r << 16) |
		(palette256[index].g << 8) |
		palette256[index].b);
}
#endif

void
get_screen_char_color(struct screen_char *schar, struct color_pair *pair,
		      enum color_flags flags, enum color_mode color_mode)
{
	unsigned char fg, bg;

	assert(color_mode >= COLOR_MODE_DUMP && color_mode < COLOR_MODES);

	/* Options for the various color modes. */
	switch (color_mode) {

	case COLOR_MODE_MONO:
		break;

	default:
		/* If the desired color mode was not compiled in,
		 * use 16 colors.  */
	case COLOR_MODE_16:
		bg = (schar->c.color[0] >> 4) & 7;
		fg = schar->c.color[0];
		pair->foreground = get_term_color16(fg);
		pair->background = get_term_color16(bg);
		break;
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
		pair->foreground = get_term_color88(schar->c.color[0]);
		pair->background = get_term_color88(schar->c.color[1]);
		break;
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
		pair->foreground = get_term_color256(schar->c.color[0]);
		pair->background = get_term_color256(schar->c.color[1]);
		break;
#endif
#ifdef CONFIG_TRUE_COLOR
	case COLOR_MODE_TRUE_COLOR:
		pair->foreground = ((schar->c.color[0] << 16)
				| (schar->c.color[1] << 8)
				| schar->c.color[2]);
		
		pair->background = ((schar->c.color[3] << 16)
				| (schar->c.color[4] << 8)
				| schar->c.color[5]);
		break;
#endif
	case COLOR_MODE_DUMP:
		break;

	case COLOR_MODES:
		/* This is caught by the assert() above. */
		break;
	}
}

void
set_term_color(struct screen_char *schar, struct color_pair *pair,
	       enum color_flags flags, enum color_mode color_mode)
{
	const struct color_mode_info *mode;
	enum palette_range palette_range = PALETTE_FULL;
	unsigned char fg, bg;

	assert(color_mode >= COLOR_MODE_DUMP && color_mode < COLOR_MODES);

	/* Options for the various color modes. */
	switch (color_mode) {

	case COLOR_MODE_MONO:
		/* TODO: A better way if possible to find out whether to
		 * inverse the fore- and backgroundcolor. Else figure out what:
		 *
		 *	CMPCODE(c) (((c) << 1 | (c) >> 2) & TERM_COLOR_MASK)
		 *
		 * mean. :) --jonas */

		/* Decrease the range of the 16 palette to not include
		 * bright colors. */
		if (flags & COLOR_DECREASE_LIGHTNESS) {
			palette_range = PALETTE_HALF;
			schar->attr |= SCREEN_ATTR_STANDOUT;
		}
		break;

	default:
		/* If the desired color mode was not compiled in,
		 * use 16 colors.  */
	case COLOR_MODE_16:
		/* Decrease the range of the 16 palette to not include
		 * bright colors. */
		if (flags & COLOR_DECREASE_LIGHTNESS)
			palette_range = PALETTE_HALF;
		break;
#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
#endif
		/* TODO: Handle decrease lightness by converting to
		 * hue-lightness-saturation color model */
		break;
#endif
#ifdef CONFIG_TRUE_COLOR
	case COLOR_MODE_TRUE_COLOR:
        	/* TODO: make it better */
        	if (pair->foreground == pair->background && (flags & COLOR_ENSURE_CONTRAST)) {
			if (flags & COLOR_ENSURE_INVERTED_CONTRAST) {
				pair->background = (pair->foreground == 0) ? 0xffffff : 0;
			} else {
				pair->foreground = (pair->background == 0) ? 0xffffff : 0;
			}
                }
		schar->c.color[0] = (pair->foreground >> 16) & 255; /* r */
		schar->c.color[1] = (pair->foreground >> 8) & 255; /* g */
		schar->c.color[2] = pair->foreground & 255; /* b */
		schar->c.color[3] = (pair->background >> 16) & 255; /* r */
		schar->c.color[4] = (pair->background >> 8) & 255; /* g */
		schar->c.color[5] = pair->background & 255; /* b */
		return;
#endif
	case COLOR_MODE_DUMP:
		return;

	case COLOR_MODES:
		/* This is caught by the assert() above. */
		return;
	}

	assert(schar);

	mode = color_modes[color_mode];
	fg = get_color(pair->foreground, mode->palette, mode->palette_range[palette_range].fg);
	bg = get_color(pair->background, mode->palette, mode->palette_range[palette_range].bg);

	switch (color_mode) {
	case COLOR_MODES:
	case COLOR_MODE_DUMP:
		INTERNAL("Bad color mode, it should _never_ occur here.");
		break;

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#ifdef CONFIG_88_COLORS
	case COLOR_MODE_88:
#endif
#ifdef CONFIG_256_COLORS
	case COLOR_MODE_256:
#endif
		/* Adjusts the foreground color to be more visible. */
		/* TODO: Be smarter! Here we just choose either black or white
		 * ANSI color to make sure the color is visible. Pasky
		 * mentioned maybe calculating a distance and choosing some
		 * intermediate color. --jonas */
		/* TODO: Maybe also do something to honour the increase_contrast
		 * option. --jonas */
		if (bg == fg && (flags & COLOR_ENSURE_CONTRAST)) {
			if (flags & COLOR_ENSURE_INVERTED_CONTRAST) {
				bg = (fg == 0) ? 15 : 0;
			} else {
				fg = (bg == 0) ? 15 : 0;
			}
		}

		TERM_COLOR_FOREGROUND_256(schar->c.color) = fg;
		TERM_COLOR_BACKGROUND_256(schar->c.color) = bg;
		break;
#endif
#ifdef CONFIG_TRUE_COLOR
	case COLOR_MODE_TRUE_COLOR:
		return;
#endif
	default:
		/* If the desired color mode was not compiled in,
		 * use 16 colors.  */
	case COLOR_MODE_MONO:
	case COLOR_MODE_16:
		set_term_color16(schar, flags, fg, bg);
		break;
	}
}
