#ifndef EL__DOCUMENT_HTML_DOMPARSER_DOMPARSER_H
#define EL__DOCUMENT_HTML_DOMPARSER_DOMPARSER_H

struct document_options;
struct html_context;
struct part;
struct string;
enum html_special_type;
enum html_element_mortality_type;


struct html_context *init_html_parser(struct uri *uri,
		       struct document_options *options,
		       unsigned char *start, unsigned char *end,
		       struct string *head, struct string *title,
		       void (*put_chars)(struct html_context *,
			                 unsigned char *, int),
		       void (*line_break)(struct html_context *),
		       void *(*special)(struct html_context *,
			                enum html_special_type, ...));
void done_html_parser(struct html_context *html_context);

void *init_html_parser_state(struct html_context *html_context,
                             enum html_element_mortality_type type,
	   	             int align, int margin, int width);
void done_html_parser_state(struct html_context *html_context,
		            void *state);

void parse_html(unsigned char *html, unsigned char *eof, struct part *part,
		unsigned char *head, struct html_context *html_context);

#endif
