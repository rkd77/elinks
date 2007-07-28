#ifndef EL__UTIL_BOX_H
#define EL__UTIL_BOX_H

/** A rectangular part of a drawing surface, such as the screen.  */
struct box {
	int x;
	int y;
	int width;
	int height;
};

/** @relates box */
static inline int
is_in_box(struct box *box, int x, int y)
{
	return (x >= box->x && y >= box->y
		&& x < box->x + box->width
		&& y < box->y + box->height);
}

/** @relates box */
static inline int
row_is_in_box(struct box *box, int y)
{
	return (y >= box->y && y < box->y + box->height);
}

/** @relates box */
static inline int
col_is_in_box(struct box *box, int x)
{
	return (x >= box->x && x < box->x + box->width);
}

/** Check whether a span of columns is in @a box.
 * Mainly intended for use with double-width characters.
 * @relates box */
static inline int
colspan_is_in_box(struct box *box, int x, int span)
{
	return (x >= box->x && x + span <= box->x + box->width);
}


/** @relates box */
static inline void
set_box(struct box *box, int x, int y, int width, int height)
{
	box->x = int_max(0, x);
	box->y = int_max(0, y);
	box->width = int_max(0, width);
	box->height = int_max(0, height);
}

/** @relates box */
static inline void
copy_box(struct box *dst, struct box *src)
{
	copy_struct(dst, src);
}

#define dbg_show_box(box) DBG("x=%i y=%i width=%i height=%i", (box)->x, (box)->y, (box)->width, (box)->height)
#define dbg_show_xy(x_, y_) DBG("x=%i y=%i", x_, y_)


#endif
