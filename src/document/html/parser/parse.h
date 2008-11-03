
#ifndef EL__DOCUMENT_HTML_PARSER_PARSE_H
#define EL__DOCUMENT_HTML_PARSER_PARSE_H

struct html_context;
struct document_options;
struct part;
struct string;

/* Flags for get_attr_value(). */
enum html_attr_flags {
	HTML_ATTR_NONE = 0,

	/* If HTML_ATTR_TEST is set then we only test for existence of
	 * an attribute of that @name. In that mode it returns NULL if
	 * attribute was not found, and a pointer to start of the attribute
	 * if it was found. */
	HTML_ATTR_TEST = 1,

	/* If HTML_ATTR_EAT_NL is not set, newline and tabs chars are
	 * replaced by spaces in returned value, else these chars are
	 * skipped. */
	HTML_ATTR_EAT_NL = 2,

	/* If HTML_ATTR_NO_CONV is set, then convert_string() is not called
	 * on value. Unused for now. */
	/* HTML_ATTR_NO_CONV = 4, */

	/* If HTML_ATTR_LITERAL_NL is set, carriage return, newline and tab
	 * characters are returned literally. */
	HTML_ATTR_LITERAL_NL = 8,
};

/* Parses html element attributes.
 * - e is attr pointer previously get from parse_element,
 * DON'T PASS HERE ANY OTHER VALUE!!!
 * - name is searched attribute
 *
 * Returns allocated string containing the attribute, or NULL on unsuccess. */
unsigned char *get_attr_value(register unsigned char *e, unsigned char *name, int cp, enum html_attr_flags flags);

/* Wrappers for get_attr_value(). */
#define get_attr_val(e, name, cp) get_attr_value(e, name, cp, HTML_ATTR_NONE)
#define get_lit_attr_val(e, name, cp) get_attr_value(e, name, cp, HTML_ATTR_LITERAL_NL)
#define get_url_val(e, name, cp) get_attr_value(e, name, cp, HTML_ATTR_EAT_NL)
#define has_attr(e, name, cp) (!!get_attr_value(e, name, cp, HTML_ATTR_TEST))


/* Interface for both the renderer and the table handling */

void parse_html(unsigned char *html, unsigned char *eof, struct part *part, unsigned char *head, struct html_context *html_context);


/* Interface for element handlers */
typedef void (element_handler_T)(struct html_context *, unsigned char *attr,
                                 unsigned char *html, unsigned char *eof,
                                 unsigned char **end);

/* Interface for the table handling */

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **);

int get_num(unsigned char *, unsigned char *, int);
int get_width(unsigned char *, unsigned char *, int, struct html_context *);

unsigned char *skip_comment(unsigned char *, unsigned char *);


void scan_http_equiv(unsigned char *s, unsigned char *eof, struct string *head, struct string *title, struct document_options *options);


/* Lifecycle functions for the tags fastfind cache, if being in use. */

void free_tags_lookup(void);
void init_tags_lookup(void);

#endif
