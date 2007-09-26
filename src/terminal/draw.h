#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "intl/charsets.h" /* unicode_val_T */

struct color_pair;
struct box;
struct terminal;

/** How many bytes we need for the colors of one character cell.  */
#if defined(CONFIG_TRUE_COLOR)
/* 0, 1, 2 - rgb foreground; 3, 4, 5 - rgb background */
#define SCREEN_COLOR_SIZE	6
#elif defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
/* 0 is foreground; 1 is background */
#define SCREEN_COLOR_SIZE	2
#else
#define SCREEN_COLOR_SIZE	1
#endif

/** Attributes of a character on the screen.
 * All attributes should fit inside an unsigned char.
 *
 * XXX: The bold mask is used as part of the color encoding. */
enum screen_char_attr {
	SCREEN_ATTR_UNSEARCHABLE = 0x01,
	SCREEN_ATTR_BOLD	= 0x08,
	SCREEN_ATTR_ITALIC	= 0x10,
	SCREEN_ATTR_UNDERLINE	= 0x20,
	SCREEN_ATTR_STANDOUT	= 0x40,
	SCREEN_ATTR_FRAME	= 0x80,
};

/** One position in the terminal screen's image. */
struct screen_char {
	/** Contains either character value or frame data.
	 * - If #attr includes ::SCREEN_ATTR_FRAME, then @c data is
	 *   enum border_char.
	 * - Otherwise, if the charset of the terminal is UTF-8, then
	 *   @c data is a character value in UCS-4.  This is possible
	 *   only if CONFIG_UTF8 is defined.
	 * - Otherwise, the charset of the terminal is assumed to be
	 *   unibyte, and @c data is a byte in that charset.  */
#ifdef CONFIG_UTF8
	unicode_val_T data;
#else
	unsigned char data;
#endif /* CONFIG_UTF8 */

	/** Attributes are ::screen_char_attr bits. */
	unsigned char attr;

	/** The fore- and background color. */
	unsigned char color[SCREEN_COLOR_SIZE];
};

/** @relates screen_char */
#define copy_screen_chars(to, from, amount) \
	do { memcpy(to, from, (amount) * sizeof(struct screen_char)); } while (0)

/** @name Linux frame symbols table.
 * It is magically converted to other terminals when needed.
 * In the screen image, they have attribute SCREEN_ATTR_FRAME;
 * you should drop them to the image using draw_border_char().
 *
 * @todo TODO: When we'll support internal Unicode, this should be
 * changed to some Unicode sequences. --pasky
 *
 * Codes extracted from twin-0.4.6 GPL project, a Textmode WINdow environment,
 * by Massimiliano Ghilardi http://linuz.sns.it/~max/
 * @{ */

/* Not yet used
#define T_UTF_16_BOX_DRAWINGS_LIGHT_VERTICAL	0x2502
#define T_UTF_16_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT	0x2524
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_SINGLE_AND_LEFT_DOUBLE	0x2561
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_LEFT_SINGLE	0x2562
#define T_UTF_16_BOX_DRAWINGS_DOWN_DOUBLE_AND_LEFT_SINGLE	0x2556
#define T_UTF_16_BOX_DRAWINGS_DOWN_SINGLE_AND_LEFT_DOUBLE	0x2555
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_LEFT	0x2563
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_VERTICAL	0x2551
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_DOWN_AND_LEFT	0x2557
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_UP_AND_LEFT	0x255D
#define T_UTF_16_BOX_DRAWINGS_UP_DOUBLE_AND_LEFT_SINGLE	0x255C
#define T_UTF_16_BOX_DRAWINGS_UP_SINGLE_AND_LEFT_DOUBLE	0x255B
#define T_UTF_16_BOX_DRAWINGS_LIGHT_DOWN_AND_LEFT	0x2510
#define T_UTF_16_BOX_DRAWINGS_LIGHT_UP_AND_RIGHT	0x2514
#define T_UTF_16_BOX_DRAWINGS_LIGHT_UP_AND_HORIZONTAL	0x2534
#define T_UTF_16_BOX_DRAWINGS_LIGHT_DOWN_AND_HORIZONTAL	0x252C
#define T_UTF_16_BOX_DRAWINGS_LIGHT_VERTICAL_AND_RIGHT	0x251C
#define T_UTF_16_BOX_DRAWINGS_LIGHT_HORIZONTAL	0x2500
#define T_UTF_16_BOX_DRAWINGS_LIGHT_VERTICAL_AND_HORIZONTAL	0x253C
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_SINGLE_AND_RIGHT_DOUBLE	0x255E
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_RIGHT_SINGLE	0x255F
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_UP_AND_RIGHT	0x255A
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_DOWN_AND_RIGHT	0x2554
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_UP_AND_HORIZONTAL	0x2569
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_DOWN_AND_HORIZONTAL	0x2566
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_RIGHT	0x2560
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_HORIZONTAL	0x2550
#define T_UTF_16_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_HORIZONTAL	0x256C
#define T_UTF_16_BOX_DRAWINGS_UP_SINGLE_AND_HORIZONTAL_DOUBLE	0x2567
#define T_UTF_16_BOX_DRAWINGS_UP_DOUBLE_AND_HORIZONTAL_SINGLE	0x2568
#define T_UTF_16_BOX_DRAWINGS_DOWN_SINGLE_AND_HORIZONTAL_DOUBLE	0x2564
#define T_UTF_16_BOX_DRAWINGS_DOWN_DOUBLE_AND_HORIZONTAL_SINGLE	0x2565
#define T_UTF_16_BOX_DRAWINGS_UP_DOUBLE_AND_RIGHT_SINGLE	0x2559
#define T_UTF_16_BOX_DRAWINGS_UP_SINGLE_AND_RIGHT_DOUBLE	0x2558
#define T_UTF_16_BOX_DRAWINGS_DOWN_SINGLE_AND_RIGHT_DOUBLE	0x2552
#define T_UTF_16_BOX_DRAWINGS_DOWN_DOUBLE_AND_RIGHT_SINGLE	0x2553
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_HORIZONTAL_SINGLE	0x256B
#define T_UTF_16_BOX_DRAWINGS_VERTICAL_SINGLE_AND_HORIZONTAL_DOUBLE	0x256A
#define T_UTF_16_BOX_DRAWINGS_LIGHT_UP_AND_LEFT	0x2518
#define T_UTF_16_BOX_DRAWINGS_LIGHT_DOWN_AND_RIGHT	0x250C
*/

/* CP437 is used by default */
#define T_CP437_BOX_DRAWINGS_LIGHT_VERTICAL	0x00B3
#define T_CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_LEFT	0x00B4
#define T_CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_LEFT_DOUBLE	0x00B5
#define T_CP437_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_LEFT_SINGLE	0x00B6
#define T_CP437_BOX_DRAWINGS_DOWN_DOUBLE_AND_LEFT_SINGLE	0x00B7
#define T_CP437_BOX_DRAWINGS_DOWN_SINGLE_AND_LEFT_DOUBLE	0x00B8
#define T_CP437_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_LEFT	0x00B9
#define T_CP437_BOX_DRAWINGS_DOUBLE_VERTICAL	0x00BA
#define T_CP437_BOX_DRAWINGS_DOUBLE_DOWN_AND_LEFT	0x00BB
#define T_CP437_BOX_DRAWINGS_DOUBLE_UP_AND_LEFT	0x00BC
#define T_CP437_BOX_DRAWINGS_UP_DOUBLE_AND_LEFT_SINGLE	0x00BD
#define T_CP437_BOX_DRAWINGS_UP_SINGLE_AND_LEFT_DOUBLE	0x00BE
#define T_CP437_BOX_DRAWINGS_LIGHT_DOWN_AND_LEFT	0x00BF
#define T_CP437_BOX_DRAWINGS_LIGHT_UP_AND_RIGHT	0x00C0
#define T_CP437_BOX_DRAWINGS_LIGHT_UP_AND_HORIZONTAL	0x00C1
#define T_CP437_BOX_DRAWINGS_LIGHT_DOWN_AND_HORIZONTAL	0x00C2
#define T_CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_RIGHT	0x00C3
#define T_CP437_BOX_DRAWINGS_LIGHT_HORIZONTAL	0x00C4
#define T_CP437_BOX_DRAWINGS_LIGHT_VERTICAL_AND_HORIZONTAL	0x00C5
#define T_CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_RIGHT_DOUBLE	0x00C6
#define T_CP437_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_RIGHT_SINGLE	0x00C7
#define T_CP437_BOX_DRAWINGS_DOUBLE_UP_AND_RIGHT	0x00C8
#define T_CP437_BOX_DRAWINGS_DOUBLE_DOWN_AND_RIGHT	0x00C9
#define T_CP437_BOX_DRAWINGS_DOUBLE_UP_AND_HORIZONTAL	0x00CA
#define T_CP437_BOX_DRAWINGS_DOUBLE_DOWN_AND_HORIZONTAL	0x00CB
#define T_CP437_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_RIGHT	0x00CC
#define T_CP437_BOX_DRAWINGS_DOUBLE_HORIZONTAL	0x00CD
#define T_CP437_BOX_DRAWINGS_DOUBLE_VERTICAL_AND_HORIZONTAL	0x00CE
#define T_CP437_BOX_DRAWINGS_UP_SINGLE_AND_HORIZONTAL_DOUBLE	0x00CF
#define T_CP437_BOX_DRAWINGS_UP_DOUBLE_AND_HORIZONTAL_SINGLE	0x00D0
#define T_CP437_BOX_DRAWINGS_DOWN_SINGLE_AND_HORIZONTAL_DOUBLE	0x00D1
#define T_CP437_BOX_DRAWINGS_DOWN_DOUBLE_AND_HORIZONTAL_SINGLE	0x00D2
#define T_CP437_BOX_DRAWINGS_UP_DOUBLE_AND_RIGHT_SINGLE	0x00D3
#define T_CP437_BOX_DRAWINGS_UP_SINGLE_AND_RIGHT_DOUBLE	0x00D4
#define T_CP437_BOX_DRAWINGS_DOWN_SINGLE_AND_RIGHT_DOUBLE	0x00D5
#define T_CP437_BOX_DRAWINGS_DOWN_DOUBLE_AND_RIGHT_SINGLE	0x00D6
#define T_CP437_BOX_DRAWINGS_VERTICAL_DOUBLE_AND_HORIZONTAL_SINGLE	0x00D7
#define T_CP437_BOX_DRAWINGS_VERTICAL_SINGLE_AND_HORIZONTAL_DOUBLE	0x00D8
#define T_CP437_BOX_DRAWINGS_LIGHT_UP_AND_LEFT	0x00D9
#define T_CP437_BOX_DRAWINGS_LIGHT_DOWN_AND_RIGHT	0x00DA

/** @} */


#define BD_LIGHT(XXX)  T_CP437_BOX_DRAWINGS_LIGHT_##XXX
#define BD_DOUBLE(XXX) T_CP437_BOX_DRAWINGS_DOUBLE_##XXX
#define BD_MIXED(XXX) 	T_CP437_BOX_DRAWINGS_##XXX

enum border_char {
	BORDER_NONE	= 0x0000,

	/* single-lined */
	BORDER_SULCORNER = BD_LIGHT(DOWN_AND_RIGHT),
	BORDER_SURCORNER = BD_LIGHT(DOWN_AND_LEFT),
	BORDER_SDLCORNER = BD_LIGHT(UP_AND_RIGHT),
	BORDER_SDRCORNER = BD_LIGHT(UP_AND_LEFT),
	BORDER_SLTEE	 = BD_LIGHT(VERTICAL_AND_LEFT), /* => the tee points to the left => -| */
	BORDER_SRTEE	 = BD_LIGHT(VERTICAL_AND_RIGHT),
	BORDER_SDTEE	 = BD_LIGHT(DOWN_AND_HORIZONTAL),
	BORDER_SUTEE	 = BD_LIGHT(UP_AND_HORIZONTAL),
	BORDER_SVLINE	 = BD_LIGHT(VERTICAL),
	BORDER_SHLINE	 = BD_LIGHT(HORIZONTAL),
	BORDER_SCROSS	 = BD_LIGHT(VERTICAL_AND_HORIZONTAL), /* + */

	/* double-lined */
	BORDER_DULCORNER = BD_DOUBLE(DOWN_AND_RIGHT),
	BORDER_DURCORNER = BD_DOUBLE(DOWN_AND_LEFT),
	BORDER_DDLCORNER = BD_DOUBLE(UP_AND_RIGHT),
	BORDER_DDRCORNER = BD_DOUBLE(UP_AND_LEFT),
	BORDER_DLTEE	 = BD_DOUBLE(VERTICAL_AND_LEFT),
	BORDER_DRTEE	 = BD_DOUBLE(VERTICAL_AND_RIGHT),
	BORDER_DDTEE	 = BD_DOUBLE(DOWN_AND_HORIZONTAL),
	BORDER_DUTEE	 = BD_DOUBLE(UP_AND_HORIZONTAL),
	BORDER_DVLINE	 = BD_DOUBLE(VERTICAL),
	BORDER_DHLINE	 = BD_DOUBLE(HORIZONTAL),
	BORDER_DCROSS	 = BD_DOUBLE(VERTICAL_AND_HORIZONTAL),

	/* Mixed single then double */
	BORDER_SDULCORNER = BD_MIXED(DOWN_SINGLE_AND_RIGHT_DOUBLE),
	BORDER_SDURCORNER = BD_MIXED(DOWN_SINGLE_AND_LEFT_DOUBLE),
	BORDER_SDDLCORNER = BD_MIXED(UP_SINGLE_AND_RIGHT_DOUBLE),
	BORDER_SDDRCORNER = BD_MIXED(UP_SINGLE_AND_LEFT_DOUBLE),
	BORDER_SDLTEE	  = BD_MIXED(VERTICAL_SINGLE_AND_LEFT_DOUBLE),
	BORDER_SDRTEE	  = BD_MIXED(VERTICAL_SINGLE_AND_RIGHT_DOUBLE),
	BORDER_SDDTEE	  = BD_MIXED(DOWN_SINGLE_AND_HORIZONTAL_DOUBLE),
	BORDER_SDUTEE	  = BD_MIXED(UP_SINGLE_AND_HORIZONTAL_DOUBLE),
	BORDER_SDCROSS	  = BD_MIXED(VERTICAL_SINGLE_AND_HORIZONTAL_DOUBLE),

	/* Mixed double then single */
	BORDER_DSULCORNER = BD_MIXED(DOWN_DOUBLE_AND_RIGHT_SINGLE),
	BORDER_DSURCORNER = BD_MIXED(DOWN_DOUBLE_AND_LEFT_SINGLE),
	BORDER_DSDLCORNER = BD_MIXED(UP_DOUBLE_AND_RIGHT_SINGLE),
	BORDER_DSDRCORNER = BD_MIXED(UP_DOUBLE_AND_LEFT_SINGLE),
	BORDER_DSLTEE	  = BD_MIXED(VERTICAL_DOUBLE_AND_LEFT_SINGLE),
	BORDER_DSRTEE	  = BD_MIXED(VERTICAL_DOUBLE_AND_RIGHT_SINGLE),
	BORDER_DSDTEE	  = BD_MIXED(DOWN_DOUBLE_AND_HORIZONTAL_SINGLE),
	BORDER_DSUTEE	  = BD_MIXED(UP_DOUBLE_AND_HORIZONTAL_SINGLE),
	BORDER_DSCROSS	  = BD_MIXED(VERTICAL_DOUBLE_AND_HORIZONTAL_SINGLE),
};

/* 0 -> 1 <- 2 v 3 ^ */
enum border_cross_direction {
	BORDER_X_RIGHT = 0,
	BORDER_X_LEFT,
	BORDER_X_DOWN,
	BORDER_X_UP
};

/** Extracts a char from the screen. */
struct screen_char *get_char(struct terminal *, int x, int y);

/** Sets the color of a screen position. */
void draw_char_color(struct terminal *term, int x, int y,
		     struct color_pair *color);

/** Sets the data of a screen position. */
#ifdef CONFIG_UTF8
void draw_char_data(struct terminal *term, int x, int y, unicode_val_T data);
#else
void draw_char_data(struct terminal *term, int x, int y, unsigned char data);
#endif /* CONFIG_UTF8 */

/** Sets the data to @a border and of a screen position. */
void draw_border_char(struct terminal *term, int x, int y,
		      enum border_char border, struct color_pair *color);

/** Sets the cross position of two borders. */
void draw_border_cross(struct terminal *, int x, int y,
		       enum border_cross_direction, struct color_pair *color);

/** Draws a char. */
#ifdef CONFIG_UTF8
void draw_char(struct terminal *term, int x, int y,
	       unicode_val_T data, enum screen_char_attr attr,
	       struct color_pair *color);
#else
void draw_char(struct terminal *term, int x, int y,
	       unsigned char data, enum screen_char_attr attr,
	       struct color_pair *color);
#endif /* CONFIG_UTF8 */

/** Draws area defined by @a box using the same colors and attributes. */
void draw_box(struct terminal *term, struct box *box,
	      unsigned char data, enum screen_char_attr attr,
	      struct color_pair *color);

/** Draws a shadow of @a width and @a height with color @a color
 * around @a box. */
void draw_shadow(struct terminal *term, struct box *box,
		 struct color_pair *color, int width, int height);

/** Draw borders. */
void draw_border(struct terminal *term, struct box *box,
		 struct color_pair *color, int width);

#ifdef CONFIG_UTF8
void fix_dwchar_around_box(struct terminal *term, struct box *box, int border,
			   int shadow_width, int shadow_height);
#endif /* CONFIG_UTF8 */

/** Draws @a length chars from @a text. */
void draw_text(struct terminal *term, int x, int y,
	       unsigned char *text, int length,
	       enum screen_char_attr attr,
	       struct color_pair *color);

/** Draws @a length chars from @a line on the screen.  */
void draw_line(struct terminal *term, int x, int y, int length,
	       struct screen_char *line);

/** Updates the terminals cursor position. When @a blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

/** Blanks the screen. */
void clear_terminal(struct terminal *);

#endif
