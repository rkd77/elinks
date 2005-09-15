/* $Id: style.h,v 1.7 2004/11/19 17:46:05 zas Exp $ */

#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

struct color_pair;
struct terminal;

/* Get the colors of the bfu element. If @color is 0 a style suitable for
 * mono terminals is returned else a style for a color terminal. */
struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename);

void done_bfu_colors(void);

#endif
