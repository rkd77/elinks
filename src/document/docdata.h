/* $Id: docdata.h,v 1.6 2004/06/12 17:28:43 zas Exp $ */

#ifndef EL__DOCUMENT_DOCDATA_H
#define EL__DOCUMENT_DOCDATA_H

#include "document/document.h"
#include "util/memory.h"

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F
#define LINK_GRANULARITY	0x7F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, struct line, LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, struct screen_char, LINE_GRANULARITY)
#define ALIGN_LINK(x, o, n) mem_align_alloc(x, o, n, struct link, LINK_GRANULARITY)

#define realloc_points(link, size) \
	mem_align_alloc(&(link)->points, (link)->npoints, size, struct point, 0)

struct line *realloc_lines(struct document *document, int y);

#endif
