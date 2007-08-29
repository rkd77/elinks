#ifndef EL__DOCUMENT_HTML_PARSER_H
#define EL__DOCUMENT_HTML_PARSER_H

/* This is generic interface for HTML parsers, as used by mainly the HTML
 * renderer. Both Mikuparser and DOM parser must conform to this. These
 * prototypes are only here, not repeated in parser-specific headers. */

struct document_options;
struct html_context;
struct part;
struct memory_list;
struct menu_item;
struct string;
enum html_special_type;


/* Major lifetime of the parser for the whole document */

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


/* Set up parser "sub-state"; the parser may be invoked on various
 * segments of the document repeatedly (in practice it is used for
 * re-parsing table contents); is_root is set for the "main" invocation. */

void *init_html_parser_state(struct html_context *html_context,
                             int is_root, int align, int margin, int width);
void done_html_parser_state(struct html_context *html_context,
		            void *state);


/* Do the parsing job, calling callbacks specified at init_html_parser()
 * time as required. */

void parse_html(unsigned char *html, unsigned char *eof, struct part *part,
		unsigned char *head, struct html_context *html_context);


/* This examines imgmap in the given document and produces a BFU-ish menu
 * for it. */

int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      struct menu_item **menu, struct memory_list **ml, struct uri *uri,
	      struct document_options *options, unsigned char *target_base,
	      int to, int def, int hdef);

#endif
