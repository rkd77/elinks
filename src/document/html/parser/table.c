/* HTML tables parser */

/* Note that this does *not* fit to the HTML parser infrastructure yet, it has
 * some special custom calling conventions and is managed from
 * document/html/tables.c. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/parse.h"
#include "document/html/parser/table.h"
#include "document/html/parser.h"
#include "document/options.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"

/* Unsafe macros */
#include "document/html/internal.h"


#define INIT_REAL_COLS		2
#define INIT_REAL_ROWS		2

#define realloc_bad_html(bad_html, size) \
	mem_align_alloc(bad_html, size, (size) + 1, 0xFF)

static void
add_table_bad_html_start(struct table *table, unsigned char *start)
{
	if (table->caption.start && !table->caption.end)
		return;

	/* Either no bad html or last one not needing @end pointer */
	if (table->bad_html_size
	    && !table->bad_html[table->bad_html_size - 1].end)
		return;

	if (realloc_bad_html(&table->bad_html, table->bad_html_size))
		table->bad_html[table->bad_html_size++].start = start;
}

static void
add_table_bad_html_end(struct table *table, unsigned char *end)
{
	if (table->caption.start && !table->caption.end) {
		table->caption.end = end;
		return;
	}

	if (table->bad_html_size
	    && !table->bad_html[table->bad_html_size - 1].end)
		table->bad_html[table->bad_html_size - 1].end = end;
}


static void
get_bordercolor(struct html_context *html_context, unsigned char *a, color_T *rgb)
{
	unsigned char *at;

	if (!use_document_fg_colors(html_context->options))
		return;

	at = get_attr_val(a, "bordercolor", html_context->doc_cp);
	/* Try some other MSIE-specific attributes if any. */
	if (!at)
		at = get_attr_val(a, "bordercolorlight", html_context->doc_cp);
	if (!at)
		at = get_attr_val(a, "bordercolordark", html_context->doc_cp);
	if (!at) return;

	decode_color(at, strlen(at), rgb);
	mem_free(at);
}

static void
get_align(struct html_context *html_context, unsigned char *attr, int *a)
{
	unsigned char *al = get_attr_val(attr, "align", html_context->doc_cp);

	if (!al) return;

	if (!c_strcasecmp(al, "left")) *a = ALIGN_LEFT;
	else if (!c_strcasecmp(al, "right")) *a = ALIGN_RIGHT;
	else if (!c_strcasecmp(al, "center")) *a = ALIGN_CENTER;
	else if (!c_strcasecmp(al, "justify")) *a = ALIGN_JUSTIFY;
	else if (!c_strcasecmp(al, "char")) *a = ALIGN_RIGHT; /* NOT IMPLEMENTED */
	mem_free(al);
}

static void
get_valign(struct html_context *html_context, unsigned char *attr, int *a)
{
	unsigned char *al = get_attr_val(attr, "valign", html_context->doc_cp);

	if (!al) return;

	if (!c_strcasecmp(al, "top")) *a = VALIGN_TOP;
	else if (!c_strcasecmp(al, "middle")) *a = VALIGN_MIDDLE;
	else if (!c_strcasecmp(al, "bottom")) *a = VALIGN_BOTTOM;
	else if (!c_strcasecmp(al, "baseline")) *a = VALIGN_BASELINE; /* NOT IMPLEMENTED */
	mem_free(al);
}

static void
get_column_width(unsigned char *attr, int *width, int sh,
                 struct html_context *html_context)
{
	unsigned char *al = get_attr_val(attr, "width", html_context->doc_cp);
	int len;

	if (!al) return;

	len = strlen(al);
	if (len && al[len - 1] == '*') {
		unsigned char *en;
		int n;

		al[len - 1] = '\0';
		errno = 0;
		n = strtoul(al, (char **) &en, 10);
		if (!errno && n >= 0 && (!*en || *en == '.'))
			*width = WIDTH_RELATIVE - n;
	} else {
		int w = get_width(attr, "width", sh, html_context);

		if (w >= 0) *width = w;
	}
	mem_free(al);
}

static void
set_table_frame(struct html_context *html_context, struct table *table,
                unsigned char *attr)
{
	unsigned char *al;

	if (!table->border) {
		table->frame = TABLE_FRAME_VOID;
		return;
	}

	table->frame = TABLE_FRAME_BOX;

	al = get_attr_val(attr, "frame", html_context->doc_cp);
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
set_table_rules(struct html_context *html_context, struct table *table,
                unsigned char *attr)
{
	unsigned char *al;

	table->rules = table->border ? TABLE_RULE_ALL : TABLE_RULE_NONE;

	al = get_attr_val(attr, "rules", html_context->doc_cp);
	if (!al) return;

	if (!c_strcasecmp(al, "none")) table->rules = TABLE_RULE_NONE;
	else if (!c_strcasecmp(al, "groups")) table->rules = TABLE_RULE_GROUPS;
	else if (!c_strcasecmp(al, "rows")) table->rules = TABLE_RULE_ROWS;
	else if (!c_strcasecmp(al, "cols")) table->rules = TABLE_RULE_COLS;
	else if (!c_strcasecmp(al, "all")) table->rules = TABLE_RULE_ALL;
	mem_free(al);
}

static void
parse_table_attributes(struct table *table, unsigned char *attr, int real,
                       struct html_context *html_context)
{
	table->fragment_id = get_attr_val(attr, "id", html_context->doc_cp);

	get_bordercolor(html_context, attr, &table->bordercolor);

	table->width = get_width(attr, "width", real, html_context);
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
	table->border = get_num(attr, "border", html_context->doc_cp);
	if (table->border == -1) {
		table->border =
		              has_attr(attr, "border", html_context->doc_cp)
			      || has_attr(attr, "rules", html_context->doc_cp)
			      || has_attr(attr, "frame", html_context->doc_cp);
	}

	if (table->border) {
		int_upper_bound(&table->border, 2);

		table->cellspacing = get_num(attr, "cellspacing", html_context->doc_cp);
		int_bounds(&table->cellspacing, 1, 2);
	}

	set_table_frame(html_context, table, attr);

	/* TODO: cellpadding may be expressed as a percentage, this is not
	 * handled yet. */
	table->cellpadding = get_num(attr, "cellpadding", html_context->doc_cp);
	if (table->cellpadding == -1) {
		table->vcellpadding = 0;
		table->cellpadding = !!table->border;
	} else {
		table->vcellpadding = (table->cellpadding >= HTML_CHAR_HEIGHT / 2 + 1);
		table->cellpadding = (table->cellpadding >= HTML_CHAR_WIDTH / 2 + 1);
	}

	set_table_rules(html_context, table, attr);

	table->align = par_format.align;
	get_align(html_context, attr, &table->align);

	table->bgcolor = par_format.bgcolor;
	get_bgcolor(html_context, attr, &table->bgcolor);
}


static struct table *
new_table(void)
{
	struct table *table = mem_calloc(1, sizeof(*table));

	if (!table) return NULL;

	table->cells = mem_calloc(INIT_REAL_COLS * INIT_REAL_ROWS,
				  sizeof(*table->cells));
	if (!table->cells) {
		mem_free(table);
		return NULL;
	}
	table->real_cols = INIT_REAL_COLS;
	table->real_rows = INIT_REAL_ROWS;

	table->columns = mem_calloc(INIT_REAL_COLS, sizeof(*table->columns));
	if (!table->columns) {
		mem_free(table->cells);
		mem_free(table);
		return NULL;
	}
	table->real_columns_count = INIT_REAL_COLS;

	return table;
}

void
free_table(struct table *table)
{
	int col, row;

	mem_free_if(table->min_cols_widths);
	mem_free_if(table->max_cols_widths);
	mem_free_if(table->cols_widths);
	mem_free_if(table->rows_heights);
	mem_free_if(table->fragment_id);
	mem_free_if(table->cols_x);
	mem_free_if(table->bad_html);

	for (col = 0; col < table->cols; col++)
		for (row = 0; row < table->rows; row++)
			mem_free_if(CELL(table, col, row)->fragment_id);

	mem_free(table->cells);
	mem_free(table->columns);
	mem_free(table);
}

static void
expand_cells(struct table *table, int dest_col, int dest_row)
{
	if (dest_col >= table->cols) {
		if (table->cols) {
			int last_col = table->cols - 1;
			int row;

			for (row = 0; row < table->rows; row++) {
				int col;
				struct table_cell *cellp = CELL(table, last_col, row);

				if (cellp->colspan != -1) continue;

				for (col = table->cols; col <= dest_col; col++) {
					struct table_cell *cell = CELL(table, col, row);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = cellp->rowspan;
					cell->colspan = -1;
					cell->col = cellp->col;
					cell->row = cellp->row;
				}
			}
		}
		table->cols = dest_col + 1;
	}

	if (dest_row >= table->rows) {
		if (table->rows) {
			int last_row = table->rows - 1;
			int col;

			for (col = 0; col < table->cols; col++) {
				int row;
				struct table_cell *cellp = CELL(table, col, last_row);

				if (cellp->rowspan != -1) continue;

				for (row = table->rows; row <= dest_row; row++) {
					struct table_cell *cell = CELL(table, col, row);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = -1;
					cell->colspan = cellp->colspan;
					cell->col = cellp->col;
					cell->row = cellp->row;
				}
			}
		}
		table->rows = dest_row + 1;
	}
}

static void
copy_table(struct table *table_src, struct table *table_dst)
{
	int row;
	int size = sizeof(*table_dst->cells) * table_src->cols;

	if (!size) return;

	for (row = 0; row < table_src->rows; row++) {
		memcpy(&table_dst->cells[row * table_dst->real_cols],
		       &table_src->cells[row * table_src->real_cols],
		       size);
	}
}


#define SMART_RAISE_LIMIT 256*1024
static inline int
smart_raise(int target, int base, int unit, int limit)
{
	while (target > base) {
		int orig_base = base;

		/* Until we reach 256kb we go fast. Then we raise
		 * by 256kb amounts. */
		if (base < limit / unit) {
			base <<= 1;
		} else {
			base += limit / unit;
		}
		/* Overflow? */
		if (base <= orig_base) return 0;
	}
	return base;
}

static struct table_cell *
new_cell(struct table *table, int dest_col, int dest_row)
{
	if (dest_col < table->cols && dest_row < table->rows)
		return CELL(table, dest_col, dest_row);

	while (1) {
		struct table new;
		int limit;

		if (dest_col < table->real_cols && dest_row < table->real_rows) {
			expand_cells(table, dest_col, dest_row);
			return CELL(table, dest_col, dest_row);
		}

		new.real_cols = smart_raise(dest_col + 1, table->real_cols,
					    sizeof(*new.cells),
					    /* assume square distribution of
					     * cols/rows */
					    SMART_RAISE_LIMIT / 2);
		if (!new.real_cols) return NULL;

		limit = SMART_RAISE_LIMIT
		      - sizeof(*new.cells) * new.real_cols;
		limit = MAX(limit, SMART_RAISE_LIMIT/2);

		new.real_rows = smart_raise(dest_row + 1, table->real_rows,
					    sizeof(*new.cells), limit);
		if (!new.real_rows) return NULL;

		new.cells = mem_calloc(new.real_cols * new.real_rows,
				       sizeof(*new.cells));
		if (!new.cells) return NULL;

		copy_table(table, &new);

		mem_free(table->cells);
		table->cells	 = new.cells;
		table->real_cols = new.real_cols;
		table->real_rows = new.real_rows;
	}
}

static void
new_columns(struct table *table, int span, int width, int align,
	    int valign, int group)
{
	if (table->columns_count + span > table->real_columns_count) {
		int n = table->real_columns_count;
		struct table_column *new_columns;

		n = smart_raise(table->columns_count + span, n,
				sizeof(*new_columns), SMART_RAISE_LIMIT);
		if (!n) return;

		new_columns = mem_realloc(table->columns, n * sizeof(*new_columns));
		if (!new_columns) return;

		table->real_columns_count = n;
		table->columns = new_columns;
	}

	while (span--) {
		struct table_column *column = &table->columns[table->columns_count++];

		column->align = align;
		column->valign = valign;
		column->width = width;
		column->group = group;
		group = 0;
	}
}

static void
set_td_width(struct table *table, int col, int width, int force)
{
	if (col >= table->cols_x_count) {
		int n = table->cols_x_count;
		int i;
		int *new_cols_x;

		n = smart_raise(col + 1, n, sizeof(*new_cols_x), SMART_RAISE_LIMIT);
		if (!n && table->cols_x_count) return;
		if (!n) n = col + 1;

		new_cols_x = mem_realloc(table->cols_x, n * sizeof(*new_cols_x));
		if (!new_cols_x) return;

		for (i = table->cols_x_count; i < n; i++)
			new_cols_x[i] = WIDTH_AUTO;
		table->cols_x_count = n;
		table->cols_x = new_cols_x;
	}

	if (force || table->cols_x[col] == WIDTH_AUTO) {
		table->cols_x[col] = width;
		return;
	}

	if (width == WIDTH_AUTO) return;

	if (width < 0 && table->cols_x[col] >= 0) {
		table->cols_x[col] = width;
		return;
	}

	if (width >= 0 && table->cols_x[col] < 0) return;
	table->cols_x[col] = (table->cols_x[col] + width) >> 1;
}

static unsigned char *
skip_table(unsigned char *html, unsigned char *eof)
{
	int level = 1;

	while (1) {
		unsigned char *name;
		int namelen, closing_tag = 0;

		while (html < eof
		       && (*html != '<'
			    || parse_element(html, eof, &name, &namelen, NULL,
					    &html)))
			html++;

		if (html >= eof) return eof;

		if (!namelen) continue;

		if (*name == '/') {
			closing_tag = 1;
			name++; namelen--;
			if (!namelen) continue;
		}


		if (!c_strlcasecmp(name, namelen, "TABLE", 5)) {
			if (!closing_tag) {
				level++;
			} else {
				level--;
				if (!level) return html;
			}
		}
	}
}

struct table *
parse_table(unsigned char *html, unsigned char *eof, unsigned char **end,
	    unsigned char *attr, int sh, struct html_context *html_context)
{
	struct table *table;
	struct table_cell *cell;
	unsigned char *t_attr, *en, *name;
	unsigned char *l_fragment_id = NULL;
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

	*end = html;

	table = new_table();
	if (!table) return NULL;

	parse_table_attributes(table, attr, sh, html_context);
	last_bgcolor = table->bgcolor;

se:
	en = html;

see:
	html = en;
	if (!in_cell) {
		add_table_bad_html_start(table, html);
	}

	while (html < eof && *html != '<') html++;

	if (html >= eof) {
		if (in_cell) CELL(table, col, row)->end = html;
		add_table_bad_html_end(table, html);
		goto scan_done;
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &name, &namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

	if (!namelen) goto see;

	if (name[0] == '/') {
		namelen--;
		if (!namelen) goto see;
	       	name++;
		closing_tag = 1;

	} else {
		closing_tag = 0;
	}

	if (!c_strlcasecmp(name, namelen, "TABLE", 5)) {
		if (!closing_tag) {
			en = skip_table(en, eof);
			goto see;

		} else {
			if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
			if (in_cell) CELL(table, col, row)->end = html;

			add_table_bad_html_end(table, html);

			goto scan_done;
		}
	}

	if (!c_strlcasecmp(name, namelen, "CAPTION", 7)) {
		if (!closing_tag) {
			add_table_bad_html_end(table, html);
			if (!table->caption.start)
				table->caption.start = html;

		} else {
			if (table->caption.start && !table->caption.end)
				table->caption.end = html;
		}

		goto see;
	}

	if (!c_strlcasecmp(name, namelen, "COLGROUP", 8)) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		add_table_bad_html_end(table, html);

		c_al = ALIGN_TR;
		c_val = VALIGN_TR;
		c_width = WIDTH_AUTO;

		if (!closing_tag) {
			get_align(html_context, t_attr, &c_al);
			get_valign(html_context, t_attr, &c_val);
			get_column_width(t_attr, &c_width, sh, html_context);
			c_span = get_num(t_attr, "span", html_context->doc_cp);
			if (c_span == -1)
				c_span = 1;
			else if (c_span > HTML_MAX_COLSPAN)
				c_span = HTML_MAX_COLSPAN;

		} else {
			c_span = 0;
		}

		goto see;
	}

	if (!closing_tag && !c_strlcasecmp(name, namelen, "COL", 3)) {
		int sp, width, al, val;

		add_table_bad_html_end(table, html);

		sp = get_num(t_attr, "span", html_context->doc_cp);
		if (sp == -1) sp = 1;
		else if (sp > HTML_MAX_COLSPAN) sp = HTML_MAX_COLSPAN;

		width = c_width;
		al = c_al;
		val = c_val;
		get_align(html_context, t_attr, &al);
		get_valign(html_context, t_attr, &val);
		get_column_width(t_attr, &width, sh, html_context);
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
				CELL(table, col, row)->end = html;
				in_cell = 0;
			}

			add_table_bad_html_end(table, html);
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

		add_table_bad_html_end(table, html);

		group = 2;
		goto see;
	}

	/* Beyond this point, only two letters tags. */
	if (namelen != 1) goto see;

	/* TR */
	if (c == 'R') {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		if (in_cell) {
			CELL(table, col, row)->end = html;
			in_cell = 0;
		}

		add_table_bad_html_end(table, html);

		if (group) group--;
		l_al = ALIGN_LEFT;
		l_val = VALIGN_MIDDLE;
		last_bgcolor = table->bgcolor;
		get_align(html_context, t_attr, &l_al);
		get_valign(html_context, t_attr, &l_val);
		get_bgcolor(html_context, t_attr, &last_bgcolor);
		mem_free_set(&l_fragment_id,
		             get_attr_val(t_attr, "id", html_context->doc_cp));
		row++;
		col = 0;
		goto see;
	}

	/* TD TH */
	is_header = (c == 'H');

	if (!is_header && c != 'D')
		goto see;

	if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

	add_table_bad_html_end(table, html);

	if (in_cell) {
		CELL(table, col, row)->end = html;
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
	cell->fragment_id = get_attr_val(t_attr, "id", html_context->doc_cp);
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

	get_align(html_context, t_attr, &cell->align);
	get_valign(html_context, t_attr, &cell->valign);
	get_bgcolor(html_context, t_attr, &cell->bgcolor);

	colspan = get_num(t_attr, "colspan", html_context->doc_cp);
	if (colspan == -1) colspan = 1;
	else if (!colspan) colspan = -1;
	else if (colspan > HTML_MAX_COLSPAN) colspan = HTML_MAX_COLSPAN;

	rowspan = get_num(t_attr, "rowspan", html_context->doc_cp);
	if (rowspan == -1) rowspan = 1;
	else if (!rowspan) rowspan = -1;
	else if (rowspan > HTML_MAX_ROWSPAN) rowspan = HTML_MAX_ROWSPAN;

	cell->colspan = colspan;
	cell->rowspan = rowspan;

	if (colspan == 1) {
		int width = WIDTH_AUTO;

		get_column_width(t_attr, &width, sh, html_context);
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
	*end = html;

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
	*end = eof;
	free_table(table);
	return NULL;
}
