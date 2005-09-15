/* $Id: location.h,v 1.1 2005/09/13 22:20:04 pasky Exp $ */

#ifndef EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H
#define EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H

#include "ecmascript/spidermonkey/util.h"

struct document_view;

extern const JSClass history_class;
extern const JSFunctionSpec history_funcs[];

extern const JSClass location_class;
extern const JSFunctionSpec location_funcs[];
extern const JSPropertySpec location_props[];

void location_goto(struct document_view *doc_view, unsigned char *url);

#endif
