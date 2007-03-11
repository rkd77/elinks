/* HTML tables renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/parse.h"
#include "document/html/parser/table.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


struct table_frames {
	unsigned int top:1;
	unsigned int bottom:1;
	unsigned int left:1;
	unsigned int right:1;
};

static void
get_table_frames(struct table *table, struct table_frames *result)
{
	assert(table && result);

	if (table->border) {
		result->top = !!(table->frame & TABLE_FRAME_ABOVE);
		result->bottom = !!(table->frame & TABLE_FRAME_BELOW);
		result->left = !!(table->frame & TABLE_FRAME_LHS);
		result->right = !!(table->frame & TABLE_FRAME_RHS);
	} else {
		memset(result, 0, sizeof(*result));
	}
}

/* Distance of the table from the left margin. */
static int
get_table_indent(struct html_context *html_context, struct table *table)
{
	int width = par_format.width - table->real_width;
	int indent;

	switch (table->align) {
	case ALIGN_CENTER:
		indent = (width + par_format.leftmargin - par_format.rightmargin) / 2;
		break;

	case ALIGN_RIGHT:
		indent = width - par_format.rightmargin;
		break;

	case ALIGN_LEFT:
	case ALIGN_JUSTIFY:
	default:
		indent = par_format.leftmargin;
	}

	/* Don't use int_bounds(&x, 0, width) here,
	 * width may be < 0. --Zas */
	if (indent > width) indent = width;
	if (indent < 0) indent = 0;

	return indent;
}

static inline struct part *
format_cell(struct html_context *html_context, struct table *table,
            struct table_cell *cell, struct document *document,
            int x, int y, int width)
{
	if (document) {
		x += table->part->box.x;
		y += table->part->box.y;
	}

	return format_html_part(html_context, cell->start, cell->end,
				cell->align, table->cellpadding, width,
	                        document, x, y, NULL, cell->link_num);
}

static inline void
get_cell_width(struct html_context *html_context,
	       unsigned char *start, unsigned char *end,
	       int cellpadding, int width,
	       int a, int *min, int *max,
	       int link_num, int *new_link_num)
{
	struct part *part;

	if (min) *min = -1;
	if (max) *max = -1;
	if (new_link_num) *new_link_num = link_num;

	part = format_html_part(html_context, start, end, ALIGN_LEFT,
				cellpadding, width, NULL,
				!!a, !!a, NULL, link_num);
	if (!part) return;

	if (min) *min = part->box.width;
	if (max) *max = part->max_width;
	if (new_link_num) *new_link_num = part->link_num;

	if (min && max) {
		assertm(*min <= *max, "get_cell_width: %d > %d", *min, *max);
	}

	mem_free(part);
}

static void
get_cell_widths(struct html_context *html_context, struct table *table)
{
	int link_num = table->part->link_num;

	if (!html_context->options->table_order) {
		int col, row;

		for (row = 0; row < table->rows; row++)
			for (col = 0; col < table->cols; col++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->start) continue;
				cell->link_num = link_num;
				get_cell_width(html_context, cell->start,
					       cell->end, table->cellpadding,
					       0, 0, &cell->min_width,
					       &cell->max_width,
					       link_num, &link_num);
			}
	} else {
		int col, row;

		for (col = 0; col < table->cols; col++)
			for (row = 0; row < table->rows; row++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->start) continue;
				cell->link_num = link_num;
				get_cell_width(html_context, cell->start,
					       cell->end, table->cellpadding,
					       0, 0, &cell->min_width,
					       &cell->max_width,
					       link_num, &link_num);
			}
	}

	table->link_num = link_num;
}

static inline void
distribute_values(int *values, int count, int wanted, int *limits)
{
	int i;
	int sum = 0, d, r, t;

	for (i = 0; i < count; i++) sum += values[i];
	if (sum >= wanted) return;

again:
	t = wanted - sum;
	d = t / count;
	r = t % count;
	wanted = 0;

	if (limits) {
		for (i = 0; i < count; i++) {
			int delta;

			values[i] += d + (i < r);

			delta = values[i] - limits[i];
			if (delta > 0) {
				wanted += delta;
				values[i] = limits[i];
			}
		}
	} else {
		for (i = 0; i < count; i++) {
			values[i] += d + (i < r);
		}
	}

	if (wanted) {
		assertm(limits != NULL, "bug in distribute_values()");
		limits = NULL;
		sum = 0;
		goto again;
	}
}


/* Returns: -1 none, 0, space, 1 line, 2 double */
static inline int
get_vline_width(struct table *table, int col)
{
	int width = 0;

	if (!col) return -1;

	if (table->rules == TABLE_RULE_COLS || table->rules == TABLE_RULE_ALL)
		width = table->cellspacing;
	else if (table->rules == TABLE_RULE_GROUPS)
		width = (col < table->columns_count && table->columns[col].group);

	if (!width && table->cellpadding) width = -1;

	return width;
}

static int
get_hline_width(struct table *table, int row)
{
	if (!row) return -1;

	if (table->rules == TABLE_RULE_ROWS || table->rules == TABLE_RULE_ALL) {
		if (table->cellspacing || table->vcellpadding)
			return table->cellspacing;
		return -1;

	} else if (table->rules == TABLE_RULE_GROUPS) {
		int col;

		for (col = 0; col < table->cols; col++)
			if (CELL(table, col, row)->is_group) {
				if (table->cellspacing || table->vcellpadding)
					return table->cellspacing;
				return -1;
			}
	}

	return table->vcellpadding ? 0 : -1;
}

#define has_vline_width(table, col) (get_vline_width(table, col) >= 0)
#define has_hline_width(table, row) (get_hline_width(table, row) >= 0)


static int
get_column_widths(struct table *table)
{
	int colspan;

	if (!table->cols) return -1; /* prevents calloc(0, ...) calls */

	if (!table->min_cols_widths) {
		table->min_cols_widths = mem_calloc(table->cols, sizeof(*table->min_cols_widths));
		if (!table->min_cols_widths) return -1;
	}

	if (!table->max_cols_widths) {
		table->max_cols_widths = mem_calloc(table->cols, sizeof(*table->max_cols_widths));
		if (!table->max_cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			return -1;
		}
	}

	if (!table->cols_widths) {
		table->cols_widths = mem_calloc(table->cols, sizeof(*table->cols_widths));
		if (!table->cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			mem_free_set(&table->max_cols_widths, NULL);
			return -1;
		}
	}

	colspan = 1;
	do {
		int col, row;
		int new_colspan = INT_MAX;

		for (col = 0; col < table->cols; col++)	for (row = 0; row < table->rows; row++) {
			struct table_cell *cell = CELL(table, col, row);

			if (cell->is_spanned || !cell->is_used) continue;

			assertm(cell->colspan + col <= table->cols, "colspan out of table");
			if_assert_failed return -1;

			if (cell->colspan == colspan) {
				int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += has_vline_width(table, col + k);

				distribute_values(&table->min_cols_widths[col],
						  colspan,
						  cell->min_width - p,
						  &table->max_cols_widths[col]);

				distribute_values(&table->max_cols_widths[col],
						  colspan,
						  cell->max_width - p,
						  NULL);

				for (k = 0; k < colspan; k++) {
					int tmp = col + k;

					int_lower_bound(&table->max_cols_widths[tmp],
							table->min_cols_widths[tmp]);
				}

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != INT_MAX);

	return 0;
}

static void
get_table_width(struct table *table)
{
	struct table_frames table_frames;
	int min = 0;
	int max = 0;
	int col;

	for (col = 0; col < table->cols; col++) {
		int vl = has_vline_width(table, col);

		min += vl + table->min_cols_widths[col];
		max += vl + table->max_cols_widths[col];
		if (table->cols_x[col] > table->max_cols_widths[col])
			max += table->cols_x[col];
	}

	get_table_frames(table, &table_frames);

	table->min_width = min + table_frames.left + table_frames.right;
	table->max_width = max + table_frames.left + table_frames.right;

	assertm(min <= max, "min(%d) > max(%d)", min, max);
	/* XXX: Recovery path? --pasky */
}


/* Initialize @width and @max_width arrays depending on the @stretch_method */
static inline int
apply_stretch_method(struct table *table, int widths[], int max_widths[],
		     int stretch_method, int max_cols_width)
{
	int col, total_width = 0;

	for (col = 0; col < table->cols; col++) {
		switch (stretch_method) {
		case 0:
			if (table->cols_widths[col] >= table->cols_x[col])
				break;

			widths[col] = 1;
			max_widths[col] = int_min(table->cols_x[col],
						  table->max_cols_widths[col])
					  - table->cols_widths[col];
			if (max_widths[col] <= 0) widths[col] = 0;
			break;
		case 1:
			if (table->cols_x[col] > WIDTH_RELATIVE)
				break;

			widths[col] = WIDTH_RELATIVE - table->cols_x[col];
			max_widths[col] = table->max_cols_widths[col]
					  - table->cols_widths[col];
			if (max_widths[col] <= 0) widths[col] = 0;
			break;
		case 2:
			if (table->cols_x[col] != WIDTH_AUTO)
				break;
			/* Fall-through */
		case 3:
			if (table->cols_widths[col] >= table->max_cols_widths[col])
				break;
			max_widths[col] = table->max_cols_widths[col]
					  - table->cols_widths[col];
			if (max_cols_width) {
				widths[col] = 5 + table->max_cols_widths[col] * 10 / max_cols_width;
			} else {
				widths[col] = 1;
			}
			break;
		case 4:
			if (table->cols_x[col] < 0)
				break;
			widths[col] = 1;
			max_widths[col] = table->cols_x[col]
					  - table->cols_widths[col];
			if (max_widths[col] <= 0) widths[col] = 0;
			break;
		case 5:
			if (table->cols_x[col] >= 0)
				break;
			if (table->cols_x[col] <= WIDTH_RELATIVE) {
				widths[col] = WIDTH_RELATIVE - table->cols_x[col];
			} else {
				widths[col] = 1;
			}
			max_widths[col] = INT_MAX;
			break;
		case 6:
			widths[col] = 1;
			max_widths[col] = INT_MAX;
			break;
		default:
			return -1;
		}

		total_width += widths[col];
	}

	return total_width;
}

/* Stretches the table columns by distributed the @spare_width among them.
 * Returns how much of @spare_width was actually distributed. */
static inline int
stretch_columns(struct table *table, int widths[], int max_widths[],
		int spare_width, int total_width)
{
	int total_spare_width = spare_width;

	while (spare_width) {
		int stretch_width = 0;
		int stretch_col = -1;
		int col;

		for (col = 0; col < table->cols; col++) {
			int col_spare_width;

			if (!widths[col])
				continue;

			col_spare_width = total_spare_width * widths[col] / total_width;
			int_bounds(&col_spare_width, 1, max_widths[col]);

			if (col_spare_width > stretch_width) {
				stretch_width = col_spare_width;
				stretch_col = col;
			}
		}

		/* Got stretch column? */
		if (stretch_col == -1)
			break;

		/* Mark the column as visited */
		widths[stretch_col] = 0;

		if (stretch_width > spare_width)
			stretch_width = spare_width;
		assertm(stretch_width >= 0, "shrinking cell");

		table->cols_widths[stretch_col] += stretch_width;
		spare_width -= stretch_width;
	}

	return total_spare_width - spare_width;
}

/* This function distributes space evenly between @table columns so that it
 * stretches to @width. */
static void
distribute_widths(struct table *table, int width)
{
	int col;
	int spare_width = width - table->min_width;
	int stretch_method = 0;
	int *widths, *max_widths;
	int max_cols_width = 0;
	int cols_array_size;

	if (!table->cols)
		return;

	assertm(spare_width >= 0, "too small width %d, required %d",
		width, table->min_width);

	for (col = 0; col < table->cols; col++)
		int_lower_bound(&max_cols_width, table->max_cols_widths[col]);

	cols_array_size = table->cols * sizeof(*table->cols_widths);
	memcpy(table->cols_widths, table->min_cols_widths, cols_array_size);
	table->real_width = width;

	widths = fmem_alloc(cols_array_size);
	if (!widths) return;

	max_widths = fmem_alloc(cols_array_size);
	if (!max_widths) goto free_widths;

	while (spare_width) {
		int stretched, total_width;

		memset(widths, 0, cols_array_size);
		memset(max_widths, 0, cols_array_size);

		total_width = apply_stretch_method(table, widths, max_widths, stretch_method, max_cols_width);
		assertm(total_width != -1, "Could not expand table");
		if_assert_failed break;

		if (!total_width) {
			stretch_method++;
			continue;
		}

		stretched = stretch_columns(table, widths, max_widths, spare_width, total_width);
		if (!stretched)
			stretch_method++;
		else
			spare_width -= stretched;
	}

	fmem_free(max_widths);
free_widths:
	fmem_free(widths);
}


static int
get_table_cellpadding(struct html_context *html_context, struct table *table)
{
	struct part *part = table->part;
	int cpd_pass = 0, cpd_width = 0, cpd_last = table->cellpadding;
	int margins = par_format.leftmargin + par_format.rightmargin;

again:
	get_cell_widths(html_context, table);
	if (get_column_widths(table)) return -1;

	get_table_width(table);

	if (!part->document && !part->box.x) {
		if (!table->full_width)
			int_upper_bound(&table->max_width, table->width);
		int_lower_bound(&table->max_width, table->min_width);

		int_lower_bound(&part->max_width, table->max_width + margins);
		int_lower_bound(&part->box.width, table->min_width + margins);

		return -1;
	}

	if (!cpd_pass && table->min_width > table->width && table->cellpadding) {
		table->cellpadding = 0;
		cpd_pass = 1;
		cpd_width = table->min_width;
		goto again;
	}
	if (cpd_pass == 1 && table->min_width > cpd_width) {
		table->cellpadding = cpd_last;
		cpd_pass = 2;
		goto again;
	}

	return 0;
}



#ifdef HTML_TABLE_2ND_PASS /* This is by default ON! (<setup.h>) */
static void
check_table_widths(struct html_context *html_context, struct table *table)
{
	int col, row;
	int colspan;
	int width, new_width;
	int max, max_index = 0; /* go away, warning! */
	int *widths = mem_calloc(table->cols, sizeof(*widths));

	if (!widths) return;

	for (row = 0; row < table->rows; row++) for (col = 0; col < table->cols; col++) {
		struct table_cell *cell = CELL(table, col, row);
		int k, p = 0;

		if (!cell->start) continue;

		for (k = 0; k < cell->colspan; k++) {
#ifdef __TINYC__
			p += table->cols_widths[col + k];
			if (k) p+= (has_vline_width(table, col + k));
#else
			p += table->cols_widths[col + k] +
			     (k && has_vline_width(table, col + k));
#endif
		}

		get_cell_width(html_context, cell->start, cell->end,
			       table->cellpadding, p, 1, &cell->width,
			       NULL, cell->link_num, NULL);

		int_upper_bound(&cell->width, p);
	}

	colspan = 1;
	do {
		int new_colspan = INT_MAX;

		for (col = 0; col < table->cols; col++) for (row = 0; row < table->rows; row++) {
			struct table_cell *cell = CELL(table, col, row);

			if (!cell->start) continue;

			assertm(cell->colspan + col <= table->cols, "colspan out of table");
			if_assert_failed goto end;

			if (cell->colspan == colspan) {
				int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += has_vline_width(table, col + k);

				distribute_values(&widths[col],
						  colspan,
						  cell->width - p,
						  &table->max_cols_widths[col]);

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != INT_MAX);

	width = new_width = 0;
	for (col = 0; col < table->cols; col++) {
		width += table->cols_widths[col];
		new_width += widths[col];
	}

	if (new_width > width) {
		/* INTERNAL("new width(%d) is larger than previous(%d)", new_width, width); */
		goto end;
	}

	max = -1;
	for (col = 0; col < table->cols; col++)
		if (table->max_cols_widths[col] > max) {
			max = table->max_cols_widths[col];
			max_index = col;
		}

	if (max != -1) {
		widths[max_index] += width - new_width;
		if (widths[max_index] <= table->max_cols_widths[max_index]) {
			mem_free(table->cols_widths);
			table->cols_widths = widths;
			return;
		}
	}

end:
	mem_free(widths);
}
#endif


static void
check_table_height(struct table *table, struct table_frames *frames, int y)
{
#ifndef CONFIG_FASTMEM
	/* XXX: Cannot we simply use the @yp value we just calculated
	 * in draw_table_cells()? --pasky */
	int old_height = table->real_height + table->part->cy;
	int our_height = frames->top + y + frames->bottom + table->caption_height;
	int row;

	/* XXX: We cannot use get_table_real_height() because we are
	 * looking one row ahead - which is completely arcane to me.
	 * It makes a difference only when a table uses ruler="groups"
	 * and has non-zero cellspacing or vcellpadding. --pasky */

	for (row = 0; row < table->rows; row++) {
#ifdef __TINYC__
		our_height += table->rows_heights[row];
		if (row < table->rows - 1 && has_hline_width(table, row + 1))
			our_height++;
#else
		our_height += table->rows_heights[row] +
			      (row < table->rows - 1 &&
			       has_hline_width(table, row + 1));
#endif
	}

	assertm(old_height == our_height, "size not matching! %d vs %d",
		old_height, our_height);
#endif
}

static int
get_table_caption_height(struct html_context *html_context, struct table *table)
{
	unsigned char *start = table->caption.start;
	unsigned char *end = table->caption.end;
	struct part *part;

	if (!start || !end) return 0;

	while (start < end && isspace(*start))
		start++;

	while (start < end && isspace(end[-1]))
		end--;

	if (start >= end) return 0;

	part = format_html_part(html_context, start, end, table->align,
		0, table->real_width, NULL, 0, 0,
		NULL, table->link_num);

	if (!part) {
		return 0;

	} else {
		int height = part->box.height;
		mem_free(part);

		return height;
	}
}

static int
get_table_real_height(struct table *table)
{
	struct table_frames table_frames;
	int height;
	int row;

	get_table_frames(table, &table_frames);

	height = table_frames.top + table_frames.bottom;
	height += table->caption_height;
	for (row = 0; row < table->rows; row++) {
		height += table->rows_heights[row];
		if (row && has_hline_width(table, row))
			height++;
	}

	return height;
}

static void
get_table_heights(struct html_context *html_context, struct table *table)
{
	int rowspan;
	int col, row;

	table->caption_height = get_table_caption_height(html_context, table);

	for (row = 0; row < table->rows; row++) {
		for (col = 0; col < table->cols; col++) {
			struct table_cell *cell = CELL(table, col, row);
			struct part *part;
			int width = 0, sp;

			if (!cell->is_used || cell->is_spanned) continue;

			for (sp = 0; sp < cell->colspan; sp++) {
#ifdef __TINYC__
				width += table->cols_widths[col + sp];
				if (sp < cell->colspan - 1)
					width += (has_vline_width(table, col + sp + 1));
#else
				width += table->cols_widths[col + sp] +
					 (sp < cell->colspan - 1 &&
					  has_vline_width(table, col + sp + 1));
#endif
			}

			part = format_cell(html_context, table, cell, NULL,
			                   2, 2, width);
			if (!part) return;

			cell->height = part->box.height;
			/* DBG("%d, %d.", width, cell->height); */
			mem_free(part);
		}
	}

	rowspan = 1;
	do {
		int new_rowspan = INT_MAX;

		for (row = 0; row < table->rows; row++) {
			for (col = 0; col < table->cols; col++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->is_used || cell->is_spanned) continue;

				if (cell->rowspan == rowspan) {
					int k, p = 0;

					for (k = 1; k < rowspan; k++)
						p += has_hline_width(table, row + k);

					distribute_values(&table->rows_heights[row],
							  rowspan,
							  cell->height - p,
							  NULL);

				} else if (cell->rowspan > rowspan &&
					   cell->rowspan < new_rowspan) {
					new_rowspan = cell->rowspan;
				}

			}
		}
		rowspan = new_rowspan;
	} while (rowspan != INT_MAX);

	table->real_height = get_table_real_height(table);
}

static void
draw_table_cell(struct table *table, int col, int row, int x, int y,
                struct html_context *html_context)
{
	struct table_cell *cell = CELL(table, col, row);
	struct document *document = table->part->document;
	struct part *part;
	int width = 0;
	int height = 0;
	int s, tmpy = y;
	struct html_element *state;

	if (!cell->start) return;

	for (s = 0; s < cell->colspan; s++) {
#ifdef __TINYC__
		width += table->cols_widths[col + s];
		if (s < cell->colspan - 1)
			width += (has_vline_width(table, col + s + 1));
#else
		width += table->cols_widths[col + s] +
			(s < cell->colspan - 1 &&
			 has_vline_width(table, col + s + 1));
#endif
	}

	for (s = 0; s < cell->rowspan; s++) {
#ifdef __TINYC__
		height += table->rows_heights[row + s];
		if (s < cell->rowspan - 1)
			height += (has_hline_width(table, row + s + 1));
#else
		height += table->rows_heights[row + s] +
			(s < cell->rowspan - 1 &&
			 has_hline_width(table, row + s + 1));
#endif
	}

	state = init_html_parser_state(html_context, ELEMENT_DONT_KILL,
	                               cell->align, 0, 0);

	if (cell->is_header) format.style.attr |= AT_BOLD;

	format.style.bg = cell->bgcolor;
	par_format.bgcolor = cell->bgcolor;

	if (cell->valign == VALIGN_MIDDLE)
		tmpy += (height - cell->height) / 2;
	else if (cell->valign == VALIGN_BOTTOM)
		tmpy += (height - cell->height);

	part = format_cell(html_context, table, cell, document, x, tmpy, width);
	if (part) {
		/* The cell content doesn't necessarily fill out the whole cell
		 * height so use the calculated @height because it is an upper
		 * bound. */
		assert(height >= cell->height);

		/* The line expansion draws the _remaining_ background color of
		 * both untouched lines and lines that doesn't stretch the
		 * whole cell width. */
		expand_lines(html_context, table->part, x + width - 1, y, height, cell->bgcolor);

		if (cell->fragment_id)
			add_fragment_identifier(html_context, part,
			                        cell->fragment_id);
	}

	done_html_parser_state(html_context, state);

	if (part) mem_free(part);
}

static void
draw_table_cells(struct table *table, int x, int y,
                 struct html_context *html_context)
{
	int col, row;
	int xp;
	color_T bgcolor = par_format.bgcolor;
	struct table_frames table_frames;

	get_table_frames(table, &table_frames);

	if (table->fragment_id)
		add_fragment_identifier(html_context, table->part,
		                        table->fragment_id);

	/* Expand using the background color of the ``parent context'' all the
	 * way down the start of the left edge of the table. */
	expand_lines(html_context, table->part, x - 1, y, table->real_height, bgcolor);

	xp = x + table_frames.left;
	for (col = 0; col < table->cols; col++) {
		int yp = y + table_frames.top;

		for (row = 0; row < table->rows; row++) {
			int row_height = table->rows_heights[row] +
				(row < table->rows - 1 && has_hline_width(table, row + 1));

			draw_table_cell(table, col, row, xp, yp, html_context);

			yp += row_height;
		}

		if (col < table->cols - 1) {
			xp += table->cols_widths[col] + has_vline_width(table, col + 1);
		}
	}

	/* Finish the table drawing by aligning the right and bottom edge of
	 * the table */
	x += table->real_width - 1;
	expand_lines(html_context, table->part, x, y, table->real_height, table->bgcolor);

	/* Tables are renderer column-wise which breaks forms where the
	 * form items appears in a column before the actual form tag is
	 * parsed. Consider the folloing example:
	 *
	 *	+--------+--------+	Where cell 2 has a <form>-tag
	 *	| cell 1 | cell 2 |	and cell 3 has an <input>-tag.
	 *	+--------+--------+
	 *	| cell 3 | cell 4 |	The table is rendered by drawing
	 *	+--------+--------+	the cells in the order: 1, 3, 2, 4.
	 *
	 * That is the <input>-tag form-item is added before the <form>-tag,
	 * which means a ``dummy'' form to hold it is added too.
	 * Calling check_html_form_hierarchy() will join the the form-item
	 * to the correct form from cell 2. */
	check_html_form_hierarchy(table->part);

	/* Do a sanity check whether the height is correct */
	check_table_height(table, &table_frames, y);
}


static inline int
get_frame_pos(int a, int a_size, int b, int b_size)
{
	assert(a >= -1 || a < a_size + 2 || b >= 0 || b <= b_size);
	if_assert_failed return 0;
	return a + 1 + (a_size + 2) * b;
}

#define H_FRAME_POSITION(table, col, row) frame[0][get_frame_pos(col, (table)->cols, row, (table)->rows)]
#define V_FRAME_POSITION(table, col, row) frame[1][get_frame_pos(row, (table)->rows, col, (table)->cols)]

static inline void
draw_frame_point(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, struct html_context *html_context)
{
	static enum border_char const border_chars[81] = {
		BORDER_NONE,		BORDER_SVLINE,		BORDER_DVLINE,
		BORDER_SHLINE,		BORDER_SDLCORNER,	BORDER_DSDLCORNER,
		BORDER_DHLINE,		BORDER_SDDLCORNER,	BORDER_DDLCORNER,

		BORDER_SHLINE,		BORDER_SDRCORNER,	BORDER_DSDRCORNER,
		BORDER_SHLINE,		BORDER_SUTEE,		BORDER_DSUTEE,
		BORDER_DHLINE,		BORDER_SDDLCORNER,	BORDER_DDLCORNER,

		BORDER_DHLINE,		BORDER_SDDRCORNER,	BORDER_DDRCORNER,
		BORDER_DHLINE,		BORDER_SDDRCORNER,	BORDER_DDRCORNER,
		BORDER_DHLINE,		BORDER_SDUTEE,		BORDER_DUTEE,

		BORDER_SVLINE,		BORDER_SVLINE,		BORDER_DVLINE,
		BORDER_SULCORNER,	BORDER_SRTEE,		BORDER_DSDLCORNER,
		BORDER_SDULCORNER,	BORDER_SDRTEE,		BORDER_DDLCORNER,

		BORDER_SURCORNER,	BORDER_SLTEE,		BORDER_DSDRCORNER,
		BORDER_SDTEE,		BORDER_SCROSS,		BORDER_DSUTEE,
		BORDER_SDULCORNER,	BORDER_SDRTEE,		BORDER_DDLCORNER,

		BORDER_SDURCORNER,	BORDER_SDLTEE,		BORDER_DDRCORNER,
		BORDER_SDURCORNER,	BORDER_SDLTEE,		BORDER_DDRCORNER,
		BORDER_SDDTEE,		BORDER_SDCROSS,		BORDER_DUTEE,

		BORDER_DVLINE,		BORDER_DVLINE,		BORDER_DVLINE,
		BORDER_DSULCORNER,	BORDER_DSULCORNER,	BORDER_DSRTEE,
		BORDER_DULCORNER,	BORDER_DULCORNER,	BORDER_DRTEE,

		BORDER_DSURCORNER,	BORDER_DSURCORNER,	BORDER_DSLTEE,
		BORDER_DSDTEE,		BORDER_DSDTEE,		BORDER_DSCROSS,
		BORDER_DULCORNER,	BORDER_DULCORNER,	BORDER_DRTEE,

		BORDER_DURCORNER,	BORDER_DURCORNER,	BORDER_DLTEE,
		BORDER_DURCORNER,	BORDER_DURCORNER,	BORDER_DLTEE,
		BORDER_DDTEE,		BORDER_DDTEE,		BORDER_DCROSS,
	};
	/* Note: I have no clue wether any of these names are suitable but they
	 * should give an idea of what is going on. --jonas */
	signed char left   = H_FRAME_POSITION(table, col - 1,     row);
	signed char right  = H_FRAME_POSITION(table,     col,     row);
	signed char top    = V_FRAME_POSITION(table,     col, row - 1);
	signed char bottom = V_FRAME_POSITION(table,     col,     row);
	int pos;

	if (left < 0 && right < 0 && top < 0 && bottom < 0) return;

	pos =      int_max(top,    0)
	    +  3 * int_max(right,  0)
	    +  9 * int_max(left,   0)
	    + 27 * int_max(bottom, 0);

	draw_frame_hchars(table->part, x, y, 1, border_chars[pos],
			  par_format.bgcolor, table->bordercolor, html_context);
}

static inline void
draw_frame_hline(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, struct html_context *html_context)
{
	static unsigned char const hltable[] = { ' ', BORDER_SHLINE, BORDER_DHLINE };
	int pos = H_FRAME_POSITION(table, col, row);

	assertm(pos < 3, "Horizontal table position out of bound [%d]", pos);
	if_assert_failed return;

	if (pos < 0 || table->cols_widths[col] <= 0) return;

	draw_frame_hchars(table->part, x, y, table->cols_widths[col], hltable[pos],
			  par_format.bgcolor, table->bordercolor, html_context);
}

static inline void
draw_frame_vline(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, struct html_context *html_context)
{
	static unsigned char const vltable[] = { ' ', BORDER_SVLINE, BORDER_DVLINE };
	int pos = V_FRAME_POSITION(table, col, row);

	assertm(pos < 3, "Vertical table position out of bound [%d]", pos);
	if_assert_failed return;

	if (pos < 0 || table->rows_heights[row] <= 0) return;

	draw_frame_vchars(table->part, x, y, table->rows_heights[row], vltable[pos],
			  par_format.bgcolor, table->bordercolor, html_context);
}

static inline int
table_row_has_group(struct table *table, int row)
{
	int col;

	for (col = 0; col < table->cols; col++)
		if (CELL(table, col, row)->is_group)
			return 1;

	return 0;
}

static void
init_table_rules(struct table *table, signed char *frame[2])
{
	int col, row;

	for (row = 0; row < table->rows; row++) for (col = 0; col < table->cols; col++) {
		int xsp, ysp;
		struct table_cell *cell = CELL(table, col, row);

		if (!cell->is_used || cell->is_spanned) continue;

		xsp = cell->colspan ? cell->colspan : table->cols - col;
		ysp = cell->rowspan ? cell->rowspan : table->rows - row;

		if (table->rules != TABLE_RULE_COLS) {
			memset(&H_FRAME_POSITION(table, col, row), table->cellspacing, xsp);
			memset(&H_FRAME_POSITION(table, col, row + ysp), table->cellspacing, xsp);
		}

		if (table->rules != TABLE_RULE_ROWS) {
			memset(&V_FRAME_POSITION(table, col, row), table->cellspacing, ysp);
			memset(&V_FRAME_POSITION(table, col + xsp, row), table->cellspacing, ysp);
		}
	}

	if (table->rules == TABLE_RULE_GROUPS) {
		for (col = 1; col < table->cols; col++) {
			if (table->cols_x[col])
				continue;

			memset(&V_FRAME_POSITION(table, col, 0), 0, table->rows);
		}

		for (row = 1; row < table->rows; row++) {
			if (table_row_has_group(table, row))
				continue;

			memset(&H_FRAME_POSITION(table, 0, row), 0, table->cols);
		}
	}
}

static void
draw_table_frames(struct table *table, int indent, int y,
                  struct html_context *html_context)
{
	struct table_frames table_frames;
	signed char *frame[2];
	int col, row;
	int cx, cy;
	int fh_size = (table->cols + 2) * (table->rows + 1);
	int fv_size = (table->cols + 1) * (table->rows + 2);

	frame[0] = fmem_alloc(fh_size + fv_size);
	if (!frame[0]) return;
	memset(frame[0], -1, fh_size + fv_size);

	frame[1] = &frame[0][fh_size];

	if (table->rules != TABLE_RULE_NONE)
		init_table_rules(table, frame);

	get_table_frames(table, &table_frames);
	memset(&H_FRAME_POSITION(table, 0, 0), table_frames.top, table->cols);
	memset(&H_FRAME_POSITION(table, 0, table->rows), table_frames.bottom, table->cols);
	memset(&V_FRAME_POSITION(table, 0, 0), table_frames.left, table->rows);
	memset(&V_FRAME_POSITION(table, table->cols, 0), table_frames.right, table->rows);

	cy = y;
	for (row = 0; row <= table->rows; row++) {
		cx = indent;
		if ((row > 0 && row < table->rows && has_hline_width(table, row))
		    || (row == 0 && table_frames.top)
		    || (row == table->rows && table_frames.bottom)) {
			int w = table_frames.left ? table->border : -1;

			for (col = 0; col < table->cols; col++) {
				if (col > 0)
					w = get_vline_width(table, col);

				if (w >= 0) {
					draw_frame_point(table, frame, cx, cy, col, row,
					                 html_context);
					if (row < table->rows)
						draw_frame_vline(table, frame, cx, cy + 1, col, row, html_context);
					cx++;
				}

				draw_frame_hline(table, frame, cx, cy, col, row,
				                 html_context);
				cx += table->cols_widths[col];
			}

			if (table_frames.right) {
				draw_frame_point(table, frame, cx, cy, col, row,
				                 html_context);
				if (row < table->rows)
					draw_frame_vline(table, frame, cx, cy + 1, col, row, html_context);
				cx++;
			}

			cy++;

		} else if (row < table->rows) {
			for (col = 0; col <= table->cols; col++) {
				if ((col > 0 && col < table->cols && has_vline_width(table, col))
				    || (col == 0 && table_frames.left)
				    || (col == table->cols && table_frames.right)) {
					draw_frame_vline(table, frame, cx, cy, col, row, html_context);
					cx++;
				}
				if (col < table->cols) cx += table->cols_widths[col];
			}
		}

		if (row < table->rows) cy += table->rows_heights[row];
	}

	fmem_free(frame[0]);
}

static void
draw_table_caption(struct html_context *html_context, struct table *table,
                   int x, int y)
{
	unsigned char *start = table->caption.start;
	unsigned char *end = table->caption.end;
	struct part *part;

	if (!start || !end) return;

	while (start < end && isspace(*start))
		start++;

	while (start < end && isspace(end[-1]))
		end--;

	if (start >= end) return;

	part = format_html_part(html_context, start, end, table->align,
		0, table->real_width, table->part->document, x, y,
		NULL, table->link_num);

	if (!part) return;

	table->part->cy += part->box.height;
	table->part->cx = -1;
	table->part->link_num = part->link_num;
	mem_free(part);
}

/* This renders tag soup elements that the parser detected while chewing it's
 * way through the table HTML. */
static void
draw_table_bad_html(struct html_context *html_context, struct table *table)
{
	int i;

	for (i = 0; i < table->bad_html_size; i++) {
		struct html_start_end *html = &table->bad_html[i];
		unsigned char *start = html->start;
		unsigned char *end = html->end;

		while (start < end && isspace(*start))
			start++;

		while (start < end && isspace(end[-1]))
			end--;

		if (start >= end) continue;

		parse_html(start, end, table->part, NULL, html_context);
	}
}

static void
distribute_table_widths(struct table *table)
{
	int width = table->width;

	if (table->min_width >= width)
		width = table->min_width;
	else if (table->max_width < width && table->full_width)
		width = table->max_width;

	distribute_widths(table, width);
}

void
format_table(unsigned char *attr, unsigned char *html, unsigned char *eof,
	     unsigned char **end, struct html_context *html_context)
{
	struct part *part = html_context->part;
	struct table *table;
	struct node *node, *new_node;
	struct html_element *state;
	int indent, margins;

	html_context->table_level++;

	table = parse_table(html, eof, end, attr, (part->document || part->box.x),
	                    html_context);
	if (!table) goto ret0;

	table->part = part;

	/* XXX: This tag soup handling needs to be done outside the create
	 * parser state. Something to do with link numbering. */
	/* It needs to be done _before_ processing the actual table, too.
	 * Otherwise i.e. <form> tags between <table> and <tr> are broken. */
	draw_table_bad_html(html_context, table);

	state = init_html_parser_state(html_context, ELEMENT_DONT_KILL,
	                               ALIGN_LEFT, 0, 0);

	margins = par_format.leftmargin + par_format.rightmargin;
	if (get_table_cellpadding(html_context, table)) goto ret2;

	distribute_table_widths(table);

	if (!part->document && part->box.x == 1) {
		int total_width = table->real_width + margins;

		int_bounds(&total_width, table->real_width, par_format.width);
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
