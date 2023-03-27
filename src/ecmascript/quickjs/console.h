#ifndef EL__ECMASCRIPT_QUICKJS_CONSOLE_H
#define EL__ECMASCRIPT_QUICKJS_CONSOLE_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_console_init(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
