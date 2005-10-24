/* Public terminal drawing API. Frontend for the screen image in memory. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/box.h"

/* Makes sure that @x and @y are within the dimensions of the terminal. */
#define check_range(term, x, y) \
	do { \
		int_bounds(&(x), 0, (term)->width - 1); \
		int_bounds(&(y), 0, (term)->height - 1); \
	} while (0)

#if defined(CONFIG_88_COLORS) || defined(CONFIG_256_COLORS)
#define clear_screen_char_color(schar) do { memset((schar)->color, 0, 2); } while (0)
#else
#define clear_screen_char_color(schar) do { (schar)->color[0] = 0; } while (0)
#endif



inline struct screen_char *
get_char(struct terminal *term, int x, int y)
{
	assert(term && term->screen && term->screen->image);
	if_assert_failed return NULL;
	check_range(term, x, y);

	return &term->screen->image[x + term->width * y];
}

void
draw_border_cross(struct terminal *term, int x, int y,
		  enum border_cross_direction dir, struct color_pair *color)
{
	static unsigned char border_trans[2][4] = {
		/* Used for BORDER_X_{RIGHT,LEFT}: */
		{ BORDER_SVLINE, BORDER_SRTEE, BORDER_SLTEE },
		/* Used for BORDER_X_{DOWN,UP}: */
		{ BORDER_SHLINE, BORDER_SDTEE, BORDER_SUTEE },
	};
	struct screen_char *screen_char = get_char(term, x, y);
	unsigned int d;

	if (!screen_char) return;
	if (!(screen_char->attr & SCREEN_ATTR_FRAME)) return;

	/* First check if there is already a horizontal/vertical line, so that
	 * we will have to replace with a T char. Example: if there is a '|'
	 * and the direction is right, replace with a '|-' T char.
	 *
	 * If this is not the case check if there is a T char and we are adding
	 * the direction so that we end up with a cross. Example : if there is
	 * a '|-' and the direction is left, replace with a '+' (cross) char. */
	d = dir>>1;
	if (screen_char->data == border_trans[d][0]) {
		screen_char->data = border_trans[d][1 + (dir & 1)];

	} else if (screen_char->data == border_trans[d][2 - (dir & 1)]) {
		screen_char->data = BORDER_SCROSS;
	}

	set_term_color(screen_char, color, 0,
		       get_opt_int_tree(term->spec, "colors"));
}

void
draw_border_char(struct terminal *term, int x, int y,
		 enum border_char border, struct color_pair *color)
{
	struct screen_char *screen_char = get_char(term, x, y);

	if (!screen_char) return;

	screen_char->data = (unsigned char) border;
	screen_char->attr = SCREEN_ATTR_FRAME;
	set_term_color(screen_char, color, 0,
		       get_opt_int_tree(term->spec, "colors"));
	set_screen_dirty(term->screen, y, y);
}

void
draw_char_color(struct terminal *term, int x, int y, struct color_pair *color)
{
	struct screen_char *screen_char = get_char(term, x, y);

	if (!screen_char) return;

	set_term_color(screen_char, color, 0,
		       get_opt_int_tree(term->spec, "colors"));
	set_screen_dirty(term->screen, y, y);
}

void
draw_char_data(struct terminal *term, int x, int y, unsigned char data)
{
	struct screen_char *screen_char = get_char(term, x, y);

	if (!screen_char) return;

	screen_char->data = data;
	set_screen_dirty(term->screen, y, y);
}

/* Updates a line in the terms screen. */
/* When doing frame drawing @x can be different than 0. */
void
draw_line(struct terminal *term, int x, int y, int l, struct screen_char *line)
{
	struct screen_char *screen_char = get_char(term, x, y);
	int size;

	assert(line);
	if_assert_failed return;
	if (!screen_char) return;

	size = int_min(l, term->width - x);
	if (size == 0) return;

	copy_screen_chars(screen_char, line, size);
	set_screen_dirty(term->screen, y, y);
}

void
draw_border(struct terminal *term, struct box *box,
	    struct color_pair *color, int width)
{
	static enum border_char p1[] = {
		BORDER_SULCORNER,
		BORDER_SURCORNER,
		BORDER_SDLCORNER,
		BORDER_SDRCORNER,
		BORDER_SVLINE,
		BORDER_SHLINE,
	};
	static enum border_char p2[] = {
		BORDER_DULCORNER,
		BORDER_DURCORNER,
		BORDER_DDLCORNER,
		BORDER_DDRCORNER,
		BORDER_DVLINE,
		BORDER_DHLINE,
	};
	enum border_char *p = (width > 1) ? p2 : p1;
	struct box borderbox;

	set_box(&borderbox, box->x - 1, box->y - 1,
		box->width + 2, box->height + 2);

	if (borderbox.width > 2) {
		struct box bbox;

		/* Horizontal top border */
		set_box(&bbox, box->x, borderbox.y, box->width, 1);
		draw_box(term, &bbox, p[5], SCREEN_ATTR_FRAME, color);

		/* Horizontal bottom border */
		bbox.y += borderbox.height - 1;
		draw_box(term, &bbox, p[5], SCREEN_ATTR_FRAME, color);
	}

	if (borderbox.height > 2) {
		struct box bbox;

		/* Vertical left border */
		set_box(&bbox, borderbox.x, box->y, 1, box->height);
		draw_box(term, &bbox, p[4], SCREEN_ATTR_FRAME, color);

		/* Vertical right border */
		bbox.x += borderbox.width - 1;
		draw_box(term, &bbox, p[4], SCREEN_ATTR_FRAME, color);
	}

	if (borderbox.width > 1 && borderbox.height > 1) {
		int right = borderbox.x + borderbox.width - 1;
		int bottom = borderbox.y + borderbox.height - 1;

		/* Upper left corner */
		draw_border_char(term, borderbox.x, borderbox.y, p[0], color);
		/* Upper right corner */
		draw_border_char(term, right, borderbox.y, p[1], color);
		/* Lower left corner */
		draw_border_char(term, borderbox.x, bottom, p[2], color);
		/* Lower right corner */
		draw_border_char(term, right, bottom, p[3], color);
	}

	set_screen_dirty(term->screen, borderbox.y, borderbox.y + borderbox.height);
}

void
draw_char(struct terminal *term, int x, int y,
	  unsigned char data, enum screen_char_attr attr,
	  struct color_pair *color)
{
	struct screen_char *screen_char = get_char(term, x, y);

	if (!screen_char) return;

	screen_char->data = data;
	screen_char->attr = attr;
	set_term_color(screen_char, color, 0,
		       get_opt_int_tree(term->spec, "colors"));

	set_screen_dirty(term->screen, y, y);
}

void
draw_box(struct terminal *term, struct box *box,
	 unsigned char data, enum screen_char_attr attr,
	 struct color_pair *color)
{
	struct screen_char *line, *pos, *end;
	int width, height;

	line = get_char(term, box->x, box->y);
	if (!line) return;

	height = int_min(box->height, term->height - box->y);
	width = int_min(box->width, term->width - box->x);

	if (height <= 0 || width <= 0) return;

	/* Compose off the ending screen position in the areas first line. */
	end = &line[width - 1];
	end->attr = attr;
	end->data = data;
	if (color) {
		set_term_color(end, color, 0,
			       get_opt_int_tree(term->spec, "colors"));
	} else {
		clear_screen_char_color(end);
	}

	/* Draw the first area line. */
	for (pos = line; pos < end; pos++) {
		copy_screen_chars(pos, end, 1);
	}

	/* Now make @end point to the last line */
	/* For the rest of the area use the first area line. */
	pos = line;
	while (--height) {
		pos += term->width;
		copy_screen_chars(pos, line, width);
	}

	set_screen_dirty(term->screen, box->y, box->y + box->height);
}

void
draw_shadow(struct terminal *term, struct box *box,
	    struct color_pair *color, int width, int height)
{
	struct box dbox;

	/* (horizontal) */
	set_box(&dbox, box->x + width, box->y + box->height,
		box->width - width, height);

	draw_box(term, &dbox, ' ', 0, color);

	/* (vertical) */
	set_box(&dbox, box->x + box->width, box->y + height,
		width, box->height);

	draw_box(term, &dbox, ' ', 0, color);
}

void
draw_text(struct terminal *term, int x, int y,
	  unsigned char *text, int length,
	  enum screen_char_attr attr, struct color_pair *color)
{
	int end_pos;
	struct screen_char *pos, *end;

	assert(text && length >= 0);
	if_assert_failed return;

	if (length <= 0) return;
	pos = get_char(term, x, y);
	if (!pos) return;

	end_pos = int_min(length, term->width - x) - 1;

#ifdef CONFIG_DEBUG
	/* Detect attempt to set @end to a point outside @text,
	 * it may occur in case of bad calculations. --Zas */
	if (end_pos < 0) {
		INTERNAL("end_pos < 0 !!");
		end_pos = 0;
	} else {
		int textlen = strlen(text);

		if (end_pos >= textlen) {
			INTERNAL("end_pos (%d) >= text length (%d) !!", end_pos, textlen);
			end_pos = textlen - 1;
		}
	}
#endif

	end = &pos[int_max(0, end_pos)];

	if (color) {
		/* Use the last char as template. */
		end->attr = attr;
		set_term_color(end, color, 0,
			       get_opt_int_tree(term->spec, "colors"));

		for (; pos < end && *text; text++, pos++) {
			end->data = *text;
			copy_screen_chars(pos, end, 1);
		}

		end->data = *text;

	} else {
		for (; pos <= end && *text; text++, pos++) {
			pos->data = *text;
		}
	}

	set_screen_dirty(term->screen, y, y);
}

void
set_cursor(struct terminal *term, int x, int y, int blockable)
{
	assert(term && term->screen);
	if_assert_failed return;

	if (blockable && get_opt_bool_tree(term->spec, "block_cursor")) {
		x = term->width - 1;
		y = term->height - 1;
	}

	if (term->screen->cx != x || term->screen->cy != y) {
		check_range(term, x, y);

		set_screen_dirty(term->screen, int_min(term->screen->cy, y),
					       int_max(term->screen->cy, y));
		term->screen->cx = x;
		term->screen->cy = y;
	}
}

void
clear_terminal(struct terminal *term)
{
	struct box box;

	set_box(&box, 0, 0, term->width, term->height);
	draw_box(term, &box, ' ', 0, NULL);
	set_cursor(term, 0, 0, 1);
}
