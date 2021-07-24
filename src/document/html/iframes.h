
#ifndef EL__DOCUMENT_HTML_IFRAMES_H
#define EL__DOCUMENT_HTML_IFRAMES_H

#ifdef __cplusplus
extern "C" {
#endif

struct iframeset_desc;
struct iframe_desc;

void add_iframeset_entry(struct iframeset_desc *parent, struct iframe_desc *subframe);

#ifdef __cplusplus
}
#endif

#endif
