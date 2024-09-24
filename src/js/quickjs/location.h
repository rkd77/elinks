#ifndef EL__ECMASCRIPT_QUICKJS_LOCATION_H
#define EL__ECMASCRIPT_QUICKJS_LOCATION_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue js_location_init(JSContext *ctx);
JSValue getLocation(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
