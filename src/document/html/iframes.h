#ifndef EL__DOCUMENT_HTML_IFRAMES_H
#define EL__DOCUMENT_HTML_IFRAMES_H

#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct document_options;
struct el_box;
struct iframeset_desc;

struct iframe2 {
	LIST_HEAD_EL(struct iframe2);
	struct el_box box;
	int number;
};

struct iframe_desc {
	char *name;
	struct uri *uri;
	struct el_box box;
	int nlink;
	int number;
	int cache_id;
	unsigned int inserted:1;
	unsigned int hidden:1;
};

struct iframeset_desc {
	int n;

	struct iframe_desc iframe_desc[1]; /* must be last of struct. --Zas */
};

void add_iframeset_entry(struct document *parent, char *url, char *name, int x, int y, int width, int height, int nlink, int hidden);
void format_iframes(struct session *ses, struct document *document, struct document_options *op, int depth);

#ifdef __cplusplus
}
#endif

#endif
