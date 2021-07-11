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
	struct html_context *html_context = renderer->html_context;
	struct part *part = html_context->part;
	struct table *table;
	struct node *node, *new_node;
	struct html_element *state;
	int indent, margins;

	html_context->table_level++;

	table = tags_parse_table(no, (part->document || part->box.x), html_context);
	if (!table) goto ret0;

	table->part = part;

	/* XXX: This tag soup handling needs to be done outside the create
	 * parser state. Something to do with link numbering. */
	/* It needs to be done _before_ processing the actual table, too.
	 * Otherwise i.e. <form> tags between <table> and <tr> are broken. */
	tags_draw_table_bad_html(html_context, table);

	state = init_html_parser_state(html_context, ELEMENT_DONT_KILL,
	                               ALIGN_LEFT, 0, 0);

	margins = /*par_elformat.blockquote_level + */par_elformat.leftmargin + par_elformat.rightmargin;
	if (get_table_cellpadding(html_context, table)) goto ret2;

	distribute_table_widths(table);

	if (!part->document && part->box.x == 1) {
		int total_width = table->real_width + margins;

		int_bounds(&total_width, table->real_width, par_elformat.width);
		int_lower_bound(&part->box.width, total_width);
		part->cy += table->real_height;

		goto ret2;
	}

#ifdef HTML_TABLE_2ND_PASS
	check_table_widths(html_context, table);
#endif

	get_table_heights(html_context, table);

	if (!part->document) {
		int_lower_bound(&part->box.width, table->real_width + margins);
		part->cy += table->real_height;
		goto ret2;
	}

	node = part->document->nodes.next;
	node->box.height = part->box.y - node->box.y + part->cy;

	indent = get_table_indent(html_context, table);

	/* FIXME: See bug 432. It should be possible to align the caption at
	 * the top, bottom or the sides. */
	draw_table_caption(html_context, table, indent + part->box.x, part->box.y + part->cy);
	draw_table_cells(table, indent, part->cy, html_context);
	draw_table_frames(table, indent, part->cy, html_context);

	part->cy += table->real_height;
	part->cx = -1;

	new_node = mem_alloc(sizeof(*new_node));
	if (new_node) {
		set_box(&new_node->box, node->box.x, part->box.y + part->cy,
			node->box.width, 0);
		add_to_list(part->document->nodes, new_node);
	}

ret2:
	part->link_num = table->link_num;
	int_lower_bound(&part->box.height, part->cy);
	html_context->part = part; /* Might've changed in draw_table_cells(). */
	done_html_parser_state(html_context, state);

	free_table(table);

ret0:
	html_context->table_level--;
	if (!html_context->table_level) free_table_cache();
}
