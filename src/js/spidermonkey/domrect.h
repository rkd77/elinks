#ifndef EL__JS_SPIDERMONKEY_DOMRECT_H
#define EL__JS_SPIDERMONKEY_DOMRECT_H

#include "js/spidermonkey/util.h"

extern JSClass domRect_class;
extern JSPropertySpec domRect_props[];
JSObject *getDomRect(JSContext *ctx, int x, int y, int width, int height, int top, int right, int bottom, int left);

#endif
