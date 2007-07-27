/** The document->data tools
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/docdata.h"
#include "document/document.h"
#include "util/error.h"


struct line *
realloc_lines(struct document *document, int y)
{
	assert(document);
	if_assert_failed return NULL;

	if (document->height <= y) {
		if (!ALIGN_LINES(&document->data, document->height, y + 1))
			return NULL;

		document->height = y + 1;
	}

	return &document->data[y];
}
