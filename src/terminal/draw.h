/* $Id: draw.h,v 1.50 2005/06/15 18:45:00 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

struct color_pair;
struct box;
struct terminal;

/* All attributes should fit inside an unsigned char. */
/* XXX: The bold mask is used as part of the color encoding. */
enum screen_char_attr {
	SCREEN_ATTR_UNSEARCHABLE = 0x01,
	SCREEN_ATTR_BOLD	= 0x08,
	SCREEN_ATTR_ITALIC	= 0x10,
	SCREEN_ATTR_UNDERLINE	= 0x20,
	SCREEN_ATTR_STANDOUT	= 0x40,
	SCREEN_ATTR_FRAME	= 0x80,
};

/* One position in the terminal screen's image. */
struct screen_char {
	/* Contains either character value or frame data. */
	unsigned char data;

	/* Attributes are screen_char_attr bits. */
	unsigned char attr;

	/* The encoded fore- and background color. */
#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
	unsigned char color[2];
#else
	unsigned char color[1];
#endif
};

#define copy_screen_chars(to, from, amount) \
	do { memcpy(to, from, (amount) * sizeof(struct screen_char)); } while (0)

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute SCREEN_ATTR_FRAME; you should drop them
 * to the image using draw_border_char(). */
/* TODO: When we'll support internal Unicode, this should be changed to some
 * Unicode sequences. --pasky */

enum border_char {
	/* single-lined */
	BORDER_SULCORNER = 218,
	BORDER_SURCORNER = 191,
	BORDER_SDLCORNER = 192,
	BORDER_SDRCORNER = 217,
	BORDER_SLTEE	 = 180, /* => the tee points to the left => -| */
	BORDER_SRTEE	 = 195,
	BORDER_SVLINE	 = 179,
	BORDER_SHLINE	 = 196,
	BORDER_SCROSS	 = 197, /* + */

	/* double-lined */ /* TODO: The TEE-chars! */
	BORDER_DULCORNER = 201,
	BORDER_DURCORNER = 187,
	BORDER_DDLCORNER = 200,
	BORDER_DDRCORNER = 188,
	BORDER_DVLINE	 = 186,
	BORDER_DHLINE	 = 205,
};

/* 0 -> 1 <- 2 v 3 ^ */
enum border_cross_direction {
	BORDER_X_RIGHT = 0,
	BORDER_X_LEFT,
	BORDER_X_DOWN,
	BORDER_X_UP
};

/* Extracts a char from the screen. */
struct screen_char *get_char(struct terminal *, int x, int y);

/* Sets the color of a screen position. */
void draw_char_color(struct terminal *term, int x, int y,
		     struct color_pair *color);

/* Sets the data of a screen position. */
void draw_char_data(struct terminal *term, int x, int y, unsigned char data);

/* Sets the data to @border and of a screen position. */
void draw_border_char(struct terminal *term, int x, int y,
		      enum border_char border, struct color_pair *color);

/* Sets the cross position of two borders. */
void draw_border_cross(struct terminal *, int x, int y,
		       enum border_cross_direction, struct color_pair *color);

/* Draws a char. */
void draw_char(struct terminal *term, int x, int y,
	       unsigned char data, enum screen_char_attr attr,
	       struct color_pair *color);

/* Draws area defined by @box using the same colors and attributes. */
void draw_box(struct terminal *term, struct box *box,
	      unsigned char data, enum screen_char_attr attr,
	      struct color_pair *color);

/* Draws a shadow of @width and @height with color @color around @box. */
void draw_shadow(struct terminal *term, struct box *box,
		 struct color_pair *color, int width, int height);

/* Draw borders. */
void draw_border(struct terminal *term, struct box *box,
		 struct color_pair *color, int width);

/* Draws @length chars from @text. */
void draw_text(struct terminal *term, int x, int y,
	       unsigned char *text, int length,
	       enum screen_char_attr attr,
	       struct color_pair *color);

/* Draws @length chars from @line on the screen. */
/* Used by viewer to copy over a document. */
void draw_line(struct terminal *term, int x, int y, int length,
	       struct screen_char *line);

/* Updates the terminals cursor position. When @blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

/* Blanks the screen. */
void clear_terminal(struct terminal *);

#endif
