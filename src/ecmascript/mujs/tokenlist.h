#ifndef EL__ECMASCRIPT_MUJS_TOKENLIST_H
#define EL__ECMASCRIPT_MUJS_TOKENLIST_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void mjs_push_tokenlist(js_State *J, void *node);

#ifdef __cplusplus
}
#endif

#endif
