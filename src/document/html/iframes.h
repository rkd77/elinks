
#ifndef EL__DOCUMENT_HTML_IFRAMES_H
#define EL__DOCUMENT_HTML_IFRAMES_H

#ifdef __cplusplus
extern "C" {
#endif

struct document_options;
struct iframeset_desc;

struct iframe_desc {
	char *name;
	struct uri *uri;

	int width, height;
};

struct iframeset_desc {
	int n;
//	struct el_box box;

	struct iframe_desc iframe_desc[1]; /* must be last of struct. --Zas */
};

void add_iframeset_entry(struct iframeset_desc **parent, char *url, char *name, int y, int width, int height);
void format_iframes(struct session *ses, struct iframeset_desc *ifsd, struct document_options *op, int depth);

#ifdef __cplusplus
}
#endif

#endif
