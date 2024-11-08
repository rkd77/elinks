#ifndef EL__JS_SPIDERMONKEY_IMAGE_H
#define EL__JS_SPIDERMONKEY_IMAGE_H

#include "js/spidermonkey/util.h"

extern JSClass image_class;
bool image_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
