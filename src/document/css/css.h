
#ifndef EL__DOCUMENT_CSS_CSS_H
#define EL__DOCUMENT_CSS_CSS_H

struct css_stylesheet;
struct module;
struct uri;

/** @todo TODO: Basicly we need two default stylesheets. One that
 * ELinks controls (which is defined by the defaults of every
 * property, they could however also be loadable at startup time,
 * e.g. when/if we will have a very generalised renderer it would be
 * possible to bypass the HTML renderer but would simply use an HTML
 * stylesheet like the one in CSS2 Appendix A. "A sample style sheet
 * for HTML 4.0") and one that the user controls. They should be
 * remerged when ever the user reloads the user stylesheet but else
 * they should be pretty static. Together they defines the basic
 * layouting should be done when rendering the document. */
extern struct css_stylesheet default_stylesheet;

extern struct module css_module;

/** This function will try to import the given @a url from the cache. */
void import_css(struct css_stylesheet *css, struct uri *uri);

int supports_css_media_type(const unsigned char *optstr,
			    const unsigned char *token, size_t token_length);

#endif
