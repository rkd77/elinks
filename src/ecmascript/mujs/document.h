#ifndef EL__ECMASCRIPT_MUJS_DOCUMENT_H
#define EL__ECMASCRIPT_MUJS_DOCUMENT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void mjs_push_document(js_State *J, void *doc);
void mjs_push_document2(js_State *J, void *doc);
int mjs_document_init(js_State *J);

#ifdef __cplusplus
}
#endif

#endif
