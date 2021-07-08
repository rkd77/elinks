/* General element handlers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "bfu/listmenu.h"
#include "bookmarks/bookmarks.h"
#include "document/css/apply.h"
#include "document/html/frames.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "document/xml/tables.h"
#include "document/xml/tags.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/align.h"
#include "util/box.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"

#include <stdio.h>

#include <libxml++/libxml++.h>

void
tags_format_table(struct source_renderer *renderer, void *no)
{
}
