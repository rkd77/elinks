#ifndef EL__JS_SPIDERMONKEY_TEXT_H
#define EL__JS_SPIDERMONKEY_TEXT_H

#include "js/spidermonkey/util.h"

extern JSClass text_class;
extern JSPropertySpec text_props[];

JSObject *getText(JSContext *ctx, void *node);

#endif
