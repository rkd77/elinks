/* $Id: color.h,v 1.6 2004/02/06 13:03:09 jonas Exp $ */

#ifndef EL__UTIL_COLOR_H
#define EL__UTIL_COLOR_H

#include "util/types.h"

typedef uint32_t color_t;

#define ALPHA_COLOR_MASK	0xFF000000
#define RED_COLOR_MASK		0x00FF0000
#define GREEN_COLOR_MASK	0x0000FF00
#define BLUE_COLOR_MASK		0x000000FF

#define RED_COLOR(color)	(((color) & RED_COLOR_MASK)   >> 16)
#define GREEN_COLOR(color)	(((color) & GREEN_COLOR_MASK) >>  8)
#define BLUE_COLOR(color)	(((color) & BLUE_COLOR_MASK)  >>  0)

struct rgb {
	unsigned char r, g, b;
	unsigned char pad;
};

/* Initialize a rgb strubt from a color_t */
#define INIT_RGB(color) \
	{ RED_COLOR(color), GREEN_COLOR(color), BLUE_COLOR(color) }

#define INT2RGB(color, rgb) \
	do { \
		(rgb).r = RED_COLOR(color); \
		(rgb).g = GREEN_COLOR(color); \
		(rgb).b = BLUE_COLOR(color); \
	} while (0)

struct color_pair {
	color_t background;
	color_t foreground;
};

#define INIT_COLOR_PAIR(bg, fg) { bg, fg }

/* Decode the color string. */
/* The color string can either contain '#FF0044' style declarations or
 * color names. */
int decode_color(unsigned char *str, int slen, color_t *color);

/* Returns a string containing the color info. If no ``English'' name can be
 * found the hex color (#rrggbb) is returned in the given buffer. */
unsigned char *get_color_string(color_t color, unsigned char hexcolor[8]);

/* Translate rgb color to string in #rrggbb format. str should be a pointer to
 * a 8 bytes memory space. */
void color_to_string(color_t color, unsigned char str[]);

/* Fastfind lookup management. */
void init_colors_lookup(void);
void free_colors_lookup(void);

#endif
