#ifndef EL__DOCUMENT_LIBDOM_CSS_H
#define EL__DOCUMENT_LIBDOM_CSS_H

#include <libcss/libcss.h>

#ifdef __cplusplus
extern "C" {
#endif

struct html_context;
struct html_element;
struct uri;

css_error resolve_url(void *pw, const char *base, lwc_string *rel, lwc_string **abs);
void select_css(struct html_context *html_context, struct html_element *element);
void parse_css(struct html_context *html_context, char *name);
void import_css2(struct html_context *html_context, struct uri *uri);

#ifdef __cplusplus
}
#endif

#endif
