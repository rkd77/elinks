#ifndef EL__JS_MUJS_NODE_H
#define EL__JS_MUJS_NODE_H

#include <mujs.h>
#include "session/download.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

int mjs_node_init(js_State *J);
void mjs_push_node(js_State *J, void *node);

#ifdef __cplusplus
}
#endif

#endif
