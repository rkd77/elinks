#ifndef EL__UTIL_COLOR_H
#define EL__UTIL_COLOR_H

typedef uint32_t color_T;

struct color_pair {
	color_T background;
	color_T foreground;
};

#define INIT_COLOR_PAIR(bg, fg) { bg, fg }

/** Decode the color string.
 * The color string can either contain '@#FF0044' style declarations or
 * color names. */
int decode_color(const unsigned char *str, int slen, color_T *color);

/** Returns a string containing the color info. If no 'English' name can be
 * found the hex color (@#rrggbb) is returned in the given buffer. */
const unsigned char *get_color_string(color_T color, unsigned char hexcolor[8]);

/** Translate rgb color to string in @#rrggbb format.
 * @a str should be a pointer to an 8 bytes memory space. */
void color_to_string(color_T color, unsigned char str[8]);

/** @name Fastfind lookup management.
 * @{ */
void init_colors_lookup(void);
void free_colors_lookup(void);
/** @} */

#endif
