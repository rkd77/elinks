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
#include "document/xml/renderer2.h"
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

static void
tags_get_valign(struct source_renderer *renderer, void *no, int *a)
{
	xmlpp::Element *node = no;
	std::string valign_value = node->get_attribute_value("valign");
	char *al = memacpy(valign_value.c_str(), valign_value.size());

	if (!al) return;

	if (!c_strcasecmp(al, "top")) *a = VALIGN_TOP;
	else if (!c_strcasecmp(al, "middle")) *a = VALIGN_MIDDLE;
	else if (!c_strcasecmp(al, "bottom")) *a = VALIGN_BOTTOM;
	else if (!c_strcasecmp(al, "baseline")) *a = VALIGN_BASELINE; /* NOT IMPLEMENTED */
	mem_free(al);
}

static void
tags_get_column_width(struct source_renderer *renderer, void *no, int *width, int sh)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	std::string width_value = node->get_attribute_value("width");
	char *al = memacpy(width_value.c_str(), width_value.size());

	int len;

	if (!al) return;

	len = strlen(al);
	if (len && al[len - 1] == '*') {
		char *en;
		int n;

		al[len - 1] = '\0';
		errno = 0;
		n = strtoul(al, (char **) &en, 10);
		if (!errno && n >= 0 && (!*en || *en == '.'))
			*width = WIDTH_RELATIVE - n;
	} else {
		int w = get_width2(al, sh, html_context);

		if (w >= 0) *width = w;
	}
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
tags_parse_table_attributes(struct source_renderer *renderer, struct table *table, void *no, int real)
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

#define realloc_bad_html(bad_html, size) \
	mem_align_alloc(bad_html, size, (size) + 1, 0xFF)

static void
tags_add_table_bad_html_start(struct table *table, void *start)
{
	if (table->caption.start_node && !table->caption.end_node)
		return;

	/* Either no bad html or last one not needing @end pointer */
	if (table->bad_html_size
	    && !table->bad_html[table->bad_html_size - 1].end_node)
		return;

	if (realloc_bad_html(&table->bad_html, table->bad_html_size))
		table->bad_html[table->bad_html_size++].start_node = start;
}

static void
tags_add_table_bad_html_end(struct table *table, void *end)
{
	if (table->caption.start_node && !table->caption.end_node) {
		table->caption.end_node = end;
		return;
	}

	if (table->bad_html_size
	    && !table->bad_html[table->bad_html_size - 1].end_node)
		table->bad_html[table->bad_html_size - 1].end_node = end;
}

static struct table *
tags_parse_table(struct source_renderer *renderer, void *no, int sh)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;

	struct table *table;
	struct table_cell *cell;
	char *t_attr, *en, *name;
	char *l_fragment_id = NULL;
	color_T last_bgcolor;
	int namelen;
	int in_cell = 0;
	int l_al = ALIGN_LEFT;
	int l_val = VALIGN_MIDDLE;
	int colspan, rowspan;
	int group = 0;
	int i, j, k;
	int c_al = ALIGN_TR, c_val = VALIGN_TR, c_width = WIDTH_AUTO, c_span = 0;
	int cols, rows;
	int col = 0, row = -1;
	int maxj;
	int closing_tag, is_header;
	unsigned char c;
	char *colspa = NULL;
	char *rowspa = NULL;
	std::string colspan_value;
	std::string rowspan_value;
	std::string id_value;
	std::string name_value;

//	*end = html;

	table = new_table();
	if (!table) return NULL;

	tags_parse_table_attributes(renderer, table, no, sh);
	last_bgcolor = table->color.background;

se:
//	en = html;

see:
//	html = en;
	if (!in_cell) {
		tags_add_table_bad_html_start(table, node);
	}

///	while (html < eof && *html != '<') html++;

	if (false) { //&& html >= eof) {
		if (in_cell) CELL(table, col, row)->end_node = node;
		tags_add_table_bad_html_end(table, node);
		goto scan_done;
	}

//	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
//		html = skip_comment(html, eof);
//		goto se;
//	}

///	if (parse_element(html, eof, &name, &namelen, &t_attr, &en)) {
///		html++;
///		goto se;
///	}
	name_value = node->get_name();
	if (name_value == "") goto see;
	name = name_value.c_str();
	namelen = name_value.size();

//	if (!namelen) goto see;

//	if (name[0] == '/') {
//		namelen--;
//		if (!namelen) goto see;
//	       	name++;
//		closing_tag = 1;
//
//	} else {
//		closing_tag = 0;
//	}
	closing_tag = 0;

//	if (!c_strlcasecmp(name, namelen, "TABLE", 5)) {
//		if (!closing_tag) {
//			en = skip_table(en, eof);
//			goto see;

	if (!c_strlcasecmp(name_value.c_str(), name_value.size(), "TABLE", 5)) {
		if (!closing_tag) {
			//en = tags_skip_table(renderer, no);
			node = node->get_next_sibling();
			goto see;
		} else {
//			if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
//			if (in_cell) CELL(table, col, row)->end = html;

//			add_table_bad_html_end(table, html);

			if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
			if (in_cell) CELL(table, col, row)->end_node = node;

			tags_add_table_bad_html_end(table, node);

			goto scan_done;
		}
	}

//	if (!c_strlcasecmp(name, namelen, "CAPTION", 7)) {
//		if (!closing_tag) {
//			add_table_bad_html_end(table, html);
//			if (!table->caption.start)
//				table->caption.start = html;

//		} else {
//			if (table->caption.start && !table->caption.end)
//				table->caption.end = html;
//		}

//		goto see;
//	}

	if (!c_strlcasecmp(name_value.c_str(), name_value.size(), "CAPTION", 7)) {
		if (!closing_tag) {
			tags_add_table_bad_html_end(table, node);
			if (!table->caption.start_node)
				table->caption.start_node = node;
		} else {
			if (table->caption.start_node && !table->caption.end_node)
				table->caption.end_node = node;
		}

		goto see;
	}

//	if (!c_strlcasecmp(name, namelen, "COLGROUP", 8)) {
//		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
//
//		add_table_bad_html_end(table, html);
//
//		c_al = ALIGN_TR;
//		c_val = VALIGN_TR;
//		c_width = WIDTH_AUTO;
//
//		if (!closing_tag) {
//			get_align(html_context, t_attr, &c_al);
//			get_valign(html_context, t_attr, &c_val);
//			get_column_width(t_attr, &c_width, sh, html_context);
//			c_span = get_num(t_attr, "span", html_context->doc_cp);
//			if (c_span == -1)
//				c_span = 1;
//			else if (c_span > HTML_MAX_COLSPAN)
//				c_span = HTML_MAX_COLSPAN;
//
//		} else {
//			c_span = 0;
//		}

//		goto see;
//	}

	if (!c_strlcasecmp(name_value.c_str(), name_value.size(), "COLGROUP", 8)) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		tags_add_table_bad_html_end(table, node);

		c_al = ALIGN_TR;
		c_val = VALIGN_TR;
		c_width = WIDTH_AUTO;

		if (!closing_tag) {
			tags_get_align(renderer, no, &c_al);
			tags_get_valign(renderer, no, &c_val);
			tags_get_column_width(renderer, no, &c_width, sh);

			std::string span_value = node->get_attribute_value("span");
			char *spa = memacpy(span_value.c_str(), span_value.size());

			c_span = get_num2(spa);
			if (c_span == -1)
				c_span = 1;
			else if (c_span > HTML_MAX_COLSPAN)
				c_span = HTML_MAX_COLSPAN;
		} else {
			c_span = 0;
		}

		goto see;
	}

//	if (!closing_tag && !c_strlcasecmp(name, namelen, "COL", 3)) {
//		int sp, width, al, val;
//
//		add_table_bad_html_end(table, html);
//
//		sp = get_num(t_attr, "span", html_context->doc_cp);
//		if (sp == -1) sp = 1;
//		else if (sp > HTML_MAX_COLSPAN) sp = HTML_MAX_COLSPAN;
//
//		width = c_width;
//		al = c_al;
//		val = c_val;
//		get_align(html_context, t_attr, &al);
//		get_valign(html_context, t_attr, &val);
//		get_column_width(t_attr, &width, sh, html_context);
//		new_columns(table, sp, width, al, val, !!c_span);
//		c_span = 0;
//		goto see;
//	}

	if (!closing_tag && !c_strlcasecmp(name_value.c_str(), name_value.size(), "COL", 3)) {
		int sp, width, al, val;

		tags_add_table_bad_html_end(table, node);

		std::string span_value = node->get_attribute_value("span");
		char *spa = memacpy(span_value.c_str(), span_value.size());

		sp = get_num2(spa);
		if (sp == -1) sp = 1;
		else if (sp > HTML_MAX_COLSPAN) sp = HTML_MAX_COLSPAN;

		width = c_width;
		al = c_al;
		val = c_val;
		tags_get_align(renderer, no, &al);
		tags_get_valign(renderer, no, &val);
		tags_get_column_width(renderer, no, &width, sh);
		new_columns(table, sp, width, al, val, !!c_span);
		c_span = 0;
		goto see;
	}

	/* All following tags have T as first letter. */
	if (c_toupper(name[0]) != 'T') goto see;

	name++; namelen--;
	if (namelen == 0) goto see;

	c = c_toupper(name[0]);

	/* /TR /TD /TH */
	if (closing_tag && namelen == 1) {
		if (c == 'R' || c == 'D' || c == 'H') {
			if (c_span)
				new_columns(table, c_span, c_width, c_al, c_val, 1);

			if (in_cell) {
				CELL(table, col, row)->end_node = node;
				in_cell = 0;
			}

			tags_add_table_bad_html_end(table, node);
			goto see;
		}
	}

	/* Beyond that point, opening tags only. */
	if (closing_tag) goto see;

	/* THEAD TBODY TFOOT */
	if (namelen == 4
	    && ((!c_strlcasecmp(name, namelen, "HEAD", 4)) ||
		(!c_strlcasecmp(name, namelen, "BODY", 4)) ||
		(!c_strlcasecmp(name, namelen, "FOOT", 4)))) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		tags_add_table_bad_html_end(table, node);

		group = 2;
		goto see;
	}

	/* Beyond this point, only two letters tags. */
	if (namelen != 1) goto see;

	/* TR */
	if (c == 'R') {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		if (in_cell) {
			CELL(table, col, row)->end_node = node;
			in_cell = 0;
		}

		tags_add_table_bad_html_end(table, node);

		if (group) group--;
		l_al = ALIGN_LEFT;
		l_val = VALIGN_MIDDLE;
		last_bgcolor = table->color.background;
		tags_get_align(renderer, no, &l_al);
		tags_get_valign(renderer, no, &l_val);
		tags_get_bgcolor(renderer, no, &last_bgcolor);
		std::string id_value = node->get_attribute_value("id");

		mem_free_set(&l_fragment_id, memacpy(id_value.c_str(), id_value.size()));
		row++;
		col = 0;
		goto see;
	}

	/* TD TH */
	is_header = (c == 'H');

	if (!is_header && c != 'D')
		goto see;

	if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

	tags_add_table_bad_html_end(table, node);

	if (in_cell) {
		CELL(table, col, row)->end_node = node;
		in_cell = 0;
	}

	if (row == -1) {
		row = 0;
		col = 0;
	}

	for (;;col++) {
		cell = new_cell(table, col, row);
		if (!cell) goto see;

		if (!cell->is_used) break;
		if (cell->colspan == -1) goto see;
	}

	in_cell = 1;

	cell->col = col;
	cell->row = row;
	cell->is_used = 1;
	cell->start = en;

	cell->align = l_al;
	cell->valign = l_val;

	id_value = node->get_attribute_value("id");
	mem_free_set(&cell->fragment_id, memacpy(id_value.c_str(), id_value.size()));

//	cell->fragment_id = get_attr_val(t_attr, "id", html_context->doc_cp);
	if (!cell->fragment_id && l_fragment_id) {
		cell->fragment_id = l_fragment_id;
		l_fragment_id = NULL;
	}

	cell->is_header = is_header;
	if (cell->is_header) cell->align = ALIGN_CENTER;

	if (group == 1) cell->is_group = 1;

	if (col < table->columns_count) {
		if (table->columns[col].align != ALIGN_TR)
			cell->align = table->columns[col].align;
		if (table->columns[col].valign != VALIGN_TR)
			cell->valign = table->columns[col].valign;
	}

	cell->bgcolor = last_bgcolor;

	tags_get_align(renderer, no, &cell->align);
	tags_get_valign(renderer, no, &cell->valign);
	tags_get_bgcolor(renderer, no, &cell->bgcolor);

	colspan_value = node->get_attribute_value("colspan");
	colspa = memacpy(colspan_value.c_str(), colspan_value.size());

	colspan = get_num2(colspa);
	if (colspan == -1) colspan = 1;
	else if (!colspan) colspan = -1;
	else if (colspan > HTML_MAX_COLSPAN) colspan = HTML_MAX_COLSPAN;

	rowspan_value = node->get_attribute_value("rowspan");
	rowspa = memacpy(rowspan_value.c_str(), rowspan_value.size());

	rowspan = get_num2(rowspa);
	if (rowspan == -1) rowspan = 1;
	else if (!rowspan) rowspan = -1;
	else if (rowspan > HTML_MAX_ROWSPAN) rowspan = HTML_MAX_ROWSPAN;

	cell->colspan = colspan;
	cell->rowspan = rowspan;

	if (colspan == 1) {
		int width = WIDTH_AUTO;

		tags_get_column_width(renderer, no, &width, sh);
		if (width != WIDTH_AUTO)
			set_td_width(table, col, width, 0);
	}

	cols = table->cols;
	for (i = 1; colspan != -1 ? i < colspan : i < cols; i++) {
		struct table_cell *span_cell = new_cell(table, col + i, row);

		if (!span_cell)
			goto abort;

		if (span_cell->is_used) {
			colspan = i;
			for (k = 0; k < i; k++)
				CELL(table, col + k, row)->colspan = colspan;
			break;
		}

		span_cell->is_used = span_cell->is_spanned = 1;
		span_cell->rowspan = rowspan;
		span_cell->colspan = colspan;
		span_cell->col = col;
		span_cell->row = row;
	}

	rows = table->rows;
	maxj = rowspan != -1 ? rowspan : rows;
	/* Out of memory prevention, limit allocated memory to HTML_MAX_CELLS_MEMORY.
	 * Not perfect but better than nothing. */
	if (maxj * i > HTML_MAX_CELLS_MEMORY / sizeof(*cell))
		goto abort;

	for (j = 1; j < maxj; j++) {
		for (k = 0; k < i; k++) {
			struct table_cell *span_cell = new_cell(table, col + k, row + j);

			if (!span_cell)
				goto abort;

			if (span_cell->is_used) {
				int l, m;

				if (span_cell->col == col
				    && span_cell->row == row)
					continue;

				for (l = 0; l < k; l++)
					memset(CELL(table, col + l, row + j), 0,
					       sizeof(*span_cell));

				rowspan = j;

				for (l = 0; l < i; l++)
					for (m = 0; m < j; m++)
						CELL(table, col + l, row + m)->rowspan = j;
				goto see;
			}

			span_cell->is_used = span_cell->is_spanned = 1;
			span_cell->rowspan = rowspan;
			span_cell->colspan = colspan;
			span_cell->col = col;
			span_cell->row = row;
		}
	}

	goto see;

scan_done:
//	*end = html;

	mem_free_if(l_fragment_id);

	for (col = 0; col < table->cols; col++) for (row = 0; row < table->rows; row++) {
		struct table_cell *cell = CELL(table, col, row);

		if (!cell->is_spanned) {
			if (cell->colspan == -1) cell->colspan = table->cols - col;
			if (cell->rowspan == -1) cell->rowspan = table->rows - row;
		}
	}

	if (table->rows) {
		table->rows_heights = mem_calloc(table->rows, sizeof(*table->rows_heights));
		if (!table->rows_heights)
			goto abort;
	} else {
		table->rows_heights = NULL;
	}

	for (col = 0; col < table->columns_count; col++)
		if (table->columns[col].width != WIDTH_AUTO)
			set_td_width(table, col, table->columns[col].width, 1);
	set_td_width(table, table->cols, WIDTH_AUTO, 0);

	return table;

abort:
//	*end = eof;
	free_table(table);
	return NULL;
}

static void
tags_draw_table_bad_html(struct source_renderer *renderer, struct table *table)
{
	struct html_context *html_context = renderer->html_context;
	int i;

	for (i = 0; i < table->bad_html_size; i++) {
		struct html_start_end *html = &table->bad_html[i];
		void *start_node = html->start_node;
		void *end_node = html->end_node;

		dump_dom_structure(renderer, start_node, 0);
	}
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

	table = tags_parse_table(renderer, no, (part->document || part->box.x));
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
