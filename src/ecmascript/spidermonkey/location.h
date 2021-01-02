
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H
#define EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H

#include "ecmascript/spidermonkey/util.h"

struct document_view;

extern JSClass history_class;
extern const spidermonkeyFunctionSpec history_funcs[];

extern JSClass location_class;
extern const spidermonkeyFunctionSpec location_funcs[];
extern JSPropertySpec location_props[];

void location_goto(struct document_view *doc_view, char *url);

#endif
