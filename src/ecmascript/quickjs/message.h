#ifndef EL__ECMASCRIPT_QUICKJS_MESSAGE_H
#define EL__ECMASCRIPT_QUICKJS_MESSAGE_H

#include <quickjs/quickjs.h>

JSValue get_messageEvent(JSContext *ctx, char *data, char *origin, char *source);

#endif
