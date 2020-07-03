
#ifndef EL__DOCUMENT_LIBDOM_GENERAL_H
#define EL__DOCUMENT_LIBDOM_GENERAL_H

#include <dom/dom.h>

struct html_context;

struct source_renderer {
	struct string tmp_buffer;
	struct string *source;
	struct html_context *html_context;
	char *enc;
};

typedef void (element_handler2_T)(struct source_renderer *, dom_node *node, unsigned char *attr,
                                 unsigned char *html, unsigned char *eof,
                                 unsigned char **end);

enum element_type {
	ET_NESTABLE,
	ET_NON_NESTABLE,
	ET_NON_PAIRABLE,
	ET_LI,
};

struct element_info2 {
	/* Element name, uppercase. */
//	unsigned char *name;

	/* Element handler. This does the relevant arguments processing and
	 * formatting (by calling renderer hooks). Note that in a few cases,
	 * this is just a placeholder and the element is given special care
	 * in start_element() (which is also where we call these handlers). */
	element_handler2_T *open;

	element_handler2_T *close;

	/* How many line-breaks to ensure we have before and after an element.
	 * Value of 1 means the element will be on a line on its own, value
	 * of 2 means that it will also have empty lines before and after.
	 * Note that this does not add up - it just ensures that there is
	 * at least so many linebreaks, but does not add more if that is the
	 * case. Therefore, something like e.g. </pre></p> will add only two
	 * linebreaks, not four. */
	/* In some stack killing logic, we use some weird heuristic based on
	 * whether an element is block or inline. That is determined from
	 * whether this attribute is zero on non-zero. */
	int linebreak;

	enum element_type type;
};

extern struct element_info2 tags_dom_html_array[];

void tags_html_apply_canvas_bgcolor(struct source_renderer *);
void tags_html_handle_body_meta(struct source_renderer *, unsigned char *, unsigned char *);

#endif
