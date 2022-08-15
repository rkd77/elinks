/* The MuJS attr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/attr.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>
#include <libxml++/attributenode.h>
#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <algorithm>
#include <string>

static void
mjs_attr_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	xmlpp::AttributeNode *attr = static_cast<xmlpp::AttributeNode *>(js_touserdata(J, 0, "attr"));

	if (!attr) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = attr->get_name();
	js_pushstring(J, v.c_str());
}

static void
mjs_attr_get_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}

	xmlpp::AttributeNode *attr = static_cast<xmlpp::AttributeNode *>(js_touserdata(J, 0, "attr"));

	if (!attr) {
		js_pushnull(J);
		return;
	}

	xmlpp::ustring v = attr->get_value();
	js_pushstring(J, v.c_str());
}

static void
mjs_attr_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[attr object]");
}

static std::map<void *, void *> map_attrs;

static
void mjs_attr_finalizer(js_State *J, void *node)
{
	map_attrs.erase(node);
}

void
mjs_push_attr(js_State *J, void *node)
{
	js_newobject(J);
	{
		js_newuserdata(J, "attr", node, mjs_attr_finalizer);
		addmethod(J, "toString", mjs_attr_toString, 0);
		addproperty(J, "name", mjs_attr_get_property_name, NULL);
		addproperty(J, "value", mjs_attr_get_property_value, NULL);
	}
}
