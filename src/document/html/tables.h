#ifndef EL__DOCUMENT_HTML_TABLES_H
#define EL__DOCUMENT_HTML_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

struct html_context;
struct table;

void distribute_table_widths(struct table *table);
void draw_table_caption(struct html_context *html_context, struct table *table, int x, int y);
void draw_table_cell(struct table *table, int col, int row, int x, int y, struct html_context *html_context);
void draw_table_cells(struct table *table, int x, int y, struct html_context *html_context);
void draw_table_frames(struct table *table, int indent, int y, struct html_context *html_context);
void format_table(char *, char *, char *, char **, struct html_context *);
int get_table_cellpadding(struct html_context *html_context, struct table *table);
void get_table_heights(struct html_context *html_context, struct table *table);
int get_table_indent(struct html_context *html_context, struct table *table);

#ifdef HTML_TABLE_2ND_PASS /* This is by default ON! (<setup.h>) */
void check_table_widths(struct html_context *html_context, struct table *table);
#endif

#ifdef __cplusplus
}
#endif

#endif
