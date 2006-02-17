#ifndef EL__DOCUMENT_DOCDATA_H
#define EL__DOCUMENT_DOCDATA_H

#include "document/document.h"
#include "util/memory.h"

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F
#define LINK_GRANULARITY	0x7F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, LINE_GRANULARITY)
#define ALIGN_LINK(x, o, n) mem_align_alloc(x, o, n, LINK_GRANULARITY)

#define realloc_points(link, size) \
	mem_align_alloc(&(link)->points, (link)->npoints, size, 0)

struct line *realloc_lines(struct document *document, int y);

#endif
