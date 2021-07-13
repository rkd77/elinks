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
#include "document/html/parser/table.h"
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

static void
get_bordercolor2(struct source_renderer *renderer, void *no, color_T *rgb)
{
	struct html_context *html_context = renderer->html_context;

	if (!use_document_fg_colors(html_context->options))
		return;

	xmlpp::Element *node = no;

	std::string color = node->get_attribute_value("bordercolor");
	if (color == "") {
		color = node->get_attribute_value("bordercolorlight");
	}
	if (color == "") {
		color = node->get_attribute_value("bordercolordark");
	}
	if (color == "") {
		return;
	}
	char *at = memacpy(color.c_str(), color.size());

	if (!at) return;

	decode_color(at, strlen(at), rgb);
	mem_free(at);
}

static void
tags_set_table_frame(struct source_renderer *renderer, void *no, struct table *table)
{
	struct html_context *html_context = renderer->html_context;
	char *al;

	if (!table->border) {
		table->frame = TABLE_FRAME_VOID;
		return;
	}

	table->frame = TABLE_FRAME_BOX;

	xmlpp::Element *node = no;
	std::string frame_value = node->get_attribute_value("frame");
	al = memacpy(frame_value.c_str(), frame_value.size());
	if (!al) return;

	if (!c_strcasecmp(al, "void")) table->frame = TABLE_FRAME_VOID;
	else if (!c_strcasecmp(al, "above")) table->frame = TABLE_FRAME_ABOVE;
	else if (!c_strcasecmp(al, "below")) table->frame = TABLE_FRAME_BELOW;
	else if (!c_strcasecmp(al, "hsides")) table->frame = TABLE_FRAME_HSIDES;
	else if (!c_strcasecmp(al, "vsides")) table->frame = TABLE_FRAME_VSIDES;
	else if (!c_strcasecmp(al, "lhs")) table->frame = TABLE_FRAME_LHS;
	else if (!c_strcasecmp(al, "rhs")) table->frame = TABLE_FRAME_RHS;
	/* Following tests are useless since TABLE_FRAME_BOX is the default.
	 * else if (!c_strcasecmp(al, "box")) table->frame = TABLE_FRAME_BOX;
	 * else if (!c_strcasecmp(al, "border")) table->frame = TABLE_FRAME_BOX;
	 */
	mem_free(al);
}

static void
tags_set_table_rules(struct source_renderer *renderer, void *no, struct table *table)
{
	char *al;

	table->rules = table->border ? TABLE_RULE_ALL : TABLE_RULE_NONE;

	xmlpp::Element *node = no;
	std::string frame_value = node->get_attribute_value("rules");
	al = memacpy(frame_value.c_str(), frame_value.size());
	if (!al) return;

	if (!c_strcasecmp(al, "none")) table->rules = TABLE_RULE_NONE;
	else if (!c_strcasecmp(al, "groups")) table->rules = TABLE_RULE_GROUPS;
	else if (!c_strcasecmp(al, "rows")) table->rules = TABLE_RULE_ROWS;
	else if (!c_strcasecmp(al, "cols")) table->rules = TABLE_RULE_COLS;
	else if (!c_strcasecmp(al, "all")) table->rules = TABLE_RULE_ALL;
	mem_free(al);
}

static void
tags_get_align(struct source_renderer *renderer, void *no, int *a)
{
	xmlpp::Element *node = no;
	std::string align_value = node->get_attribute_value("align");
	char *al = memacpy(align_value.c_str(), align_value.size());

	if (!al) return;

	if (!c_strcasecmp(al, "left")) *a = ALIGN_LEFT;
	else if (!c_strcasecmp(al, "right")) *a = ALIGN_RIGHT;
	else if (!c_strcasecmp(al, "center")) *a = ALIGN_CENTER;
	else if (!c_strcasecmp(al, "justify")) *a = ALIGN_JUSTIFY;
	else if (!c_strcasecmp(al, "char")) *a = ALIGN_RIGHT; /* NOT IMPLEMENTED */
	mem_free(al);
}

int
tags_get_bgcolor(struct source_renderer *renderer, void *no, color_T *rgb)
{
	struct html_context *html_context = renderer->html_context;
	if (!use_document_bg_colors(html_context->options))
		return -1;

	xmlpp::Element *node = no;
	std::string bgcolor_value = node->get_attribute_value("bgcolor");
	char *at = memacpy(bgcolor_value.c_str(), bgcolor_value.size());

	if (!at) return -1;
	int res = decode_color(at, strlen(at), rgb);
	mem_free(at);

	return res;
}



static void
tags_parse_table_attributes(struct table *table, struct source_renderer *renderer, void *no, int real)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;

	std::string id_value = node->get_attribute_value("id");

	if (id_value != "") {
		table->fragment_id = memacpy(id_value.c_str(), id_value.size());
	}

	get_bordercolor2(renderer, no, &table->color.border);
	std::string width_value = node->get_attribute_value("width");
	char *width = memacpy(width_value.c_str(), width_value.size());

	table->width = get_width2(width, real, html_context);
	if (table->width == -1) {
		table->width = get_html_max_width();
		table->full_width = 1;
	}

	/* From http://www.w3.org/TR/html4/struct/tables.html#adef-border-TABLE
	 * The following settings should be observed by user agents for
	 * backwards compatibility.
	 * Setting border="0" implies frame="void" and, unless otherwise
	 * specified, rules="none".
	 * Other values of border imply frame="border" and, unless otherwise
	 * specified, rules="all".
	 * The value "border" in the start tag of the TABLE element should be
	 * interpreted as the value of the frame attribute. It implies
	 * rules="all" and some default (non-zero) value for the border
	 * attribute. */
	std::string border_value = node->get_attribute_value("border");
	if (border_value != "") {
		char *at = memacpy(border_value.c_str(), border_value.size());
		table->border = get_num2(at);
	}
	if (table->border == -1) {
		xmlpp::Attribute *attr = dynamic_cast<xmlpp::Attribute *>(node->get_attribute("border"));
		if (!attr) {
			attr = dynamic_cast<xmlpp::Attribute *>(node->get_attribute("rules"));
		}
		if (!attr) {
			attr = dynamic_cast<xmlpp::Attribute *>(node->get_attribute("frames"));
		}
		table->border = (attr != nullptr);
	}

	if (table->border) {
		int_upper_bound(&table->border, 2);

		std::string cellspacing_value = node->get_attribute_value("cellspacing");

		if (cellspacing_value != "") {
			char *at = memacpy(cellspacing_value.c_str(), cellspacing_value.size());
			table->cellspacing = get_num2(at);
		}
		int_bounds(&table->cellspacing, 1, 2);
	}

	tags_set_table_frame(renderer, no, table);

	/* TODO: cellpadding may be expressed as a percentage, this is not
	 * handled yet. */
	std::string cellpadding_value = node->get_attribute_value("cellpadding");
	char *at = memacpy(cellpadding_value.c_str(), cellpadding_value.size());
	table->cellpadding = get_num2(at);
	if (table->cellpadding == -1) {
		table->vcellpadding = 0;
		table->cellpadding = !!table->border;
	} else {
		table->vcellpadding = (table->cellpadding >= HTML_CHAR_HEIGHT / 2 + 1);
		table->cellpadding = (table->cellpadding >= HTML_CHAR_WIDTH / 2 + 1);
	}

	tags_set_table_rules(renderer, no, table);

	table->align = par_elformat.align;
	tags_get_align(renderer, no, &table->align);

	table->color.background = par_elformat.color.background;
	tags_get_bgcolor(renderer, no, &table->color.background);
}

static struct table *
tags_parse_table(struct source_renderer *renderer, int t, void *no)
{
	struct html_context *html_context = renderer->html_context;

	return NULL;
}

static void
tags_draw_table_bad_html(struct source_renderer *renderer, struct table *table)
{
	struct html_context *html_context = renderer->html_context;
}

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

	table = tags_parse_table(renderer, (part->document || part->box.x), no);
	if (!table) goto ret0;

	table->part = part;

	/* XXX: This tag soup handling needs to be done outside the create
	 * parser state. Something to do with link numbering. */
	/* It needs to be done _before_ processing the actual table, too.
	 * Otherwise i.e. <form> tags between <table> and <tr> are broken. */
	tags_draw_table_bad_html(renderer, table);

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
