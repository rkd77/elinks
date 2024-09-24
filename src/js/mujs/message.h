#ifndef EL__JS_MUJS_MESSAGE_H
#define EL__JS_MUJS_MESSAGE_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

int mjs_messageEvent_init(js_State *J);
void mjs_push_messageEvent(js_State *J, char *data, char *origin, char *source);

#ifdef __cplusplus
}
#endif

#endif
