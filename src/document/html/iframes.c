/* HTML frames parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/html/iframes.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/string.h"
#include "util/time.h"

void add_iframeset_entry(struct iframeset_desc **parent,
		char *url, int y, int height)
{
	struct iframeset_desc *iframeset_desc;
	struct iframe_desc *iframe_desc;
	int offset;

	assert(parent);
	if_assert_failed return;

	if (!*parent) {
		*parent = mem_calloc(1, sizeof(iframeset_desc));
	} else {
		*parent = mem_realloc(*parent, sizeof(struct iframeset_desc) + ((*parent)->n + 1) * sizeof(struct iframe_desc));
	}
	if (!*parent) return;

	iframeset_desc = *parent;

	offset = iframeset_desc->n;
	iframe_desc = &iframeset_desc->iframe_desc[offset];
	iframe_desc->name = stracpy("");
	iframe_desc->uri = get_uri(url, 0);
	iframe_desc->height = height;
	if (!iframe_desc->uri)
		iframe_desc->uri = get_uri("about:blank", 0);

	iframeset_desc->n++;
}
