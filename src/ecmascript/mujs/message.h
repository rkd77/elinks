#ifndef EL__ECMASCRIPT_MUJS_MESSAGE_H
#define EL__ECMASCRIPT_MUJS_MESSAGE_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void mjs_push_messageEvent(js_State *J, char *data, char *origin, char *source);

#ifdef __cplusplus
}
#endif

#endif
