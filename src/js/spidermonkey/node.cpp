/* The SpiderMonkey Node implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "js/spidermonkey/util.h"
#include <js/BigInt.h>
#include <js/Conversions.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/spidermonkey.h"
#include "js/spidermonkey/attr.h"
#include "js/spidermonkey/document.h"
#include "js/spidermonkey/element.h"
#include "js/spidermonkey/fragment.h"
#include "js/spidermonkey/heartbeat.h"
#include "js/spidermonkey/node.h"
#include "js/spidermonkey/text.h"
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
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

enum {
	ELEMENT_NODE = 1,
	ATTRIBUTE_NODE = 2,
	TEXT_NODE = 3,
	CDATA_SECTION_NODE = 4,
	PROCESSING_INSTRUCTION_NODE = 7,
	COMMENT_NODE = 8,
	DOCUMENT_NODE = 9,
	DOCUMENT_TYPE_NODE = 10,
	DOCUMENT_FRAGMENT_NODE = 11
};


JSClassOps node_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass node_class = {
	"Node",
	JSCLASS_HAS_RESERVED_SLOTS(0),
	&node_ops
};

JSObject *
getNode(JSContext *ctx, void *n)
{
	dom_node *node = (dom_node *)n;
	dom_node_type typ;
	dom_exception exc = dom_node_get_node_type(node, &typ);

	if (exc != DOM_NO_ERR) {
		return NULL;
	}
	switch (typ) {
	case ELEMENT_NODE:
		return getElement(ctx, n);
	case ATTRIBUTE_NODE:
		return getAttr(ctx, n);
	case TEXT_NODE:
	case CDATA_SECTION_NODE:
	case PROCESSING_INSTRUCTION_NODE:
	case COMMENT_NODE:
	default:
		return getText(ctx, n);
	case DOCUMENT_NODE:
		return getDocument(ctx, n);
	case DOCUMENT_TYPE_NODE:
		return getDoctype(ctx, n);
	case DOCUMENT_FRAGMENT_NODE:
		return getDocumentFragment(ctx, n);
	}
}

bool
node_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setNull();

	return true;
}

static bool
node_static_get_property_ELEMENT_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(ELEMENT_NODE);

	return true;
}

static bool
node_static_get_property_ATTRIBUTE_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(ATTRIBUTE_NODE);

	return true;
}

static bool
node_static_get_property_TEXT_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(TEXT_NODE);

	return true;
}

static bool
node_static_get_property_CDATA_SECTION_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(CDATA_SECTION_NODE);

	return true;
}

static bool
node_static_get_property_PROCESSING_INSTRUCTION_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(PROCESSING_INSTRUCTION_NODE);

	return true;
}

static bool
node_static_get_property_COMMENT_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(COMMENT_NODE);

	return true;
}

static bool
node_static_get_property_DOCUMENT_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(DOCUMENT_NODE);

	return true;
}

static bool
node_static_get_property_DOCUMENT_TYPE_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(DOCUMENT_TYPE_NODE);

	return true;
}

static bool
node_static_get_property_DOCUMENT_FRAGMENT_NODE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(DOCUMENT_FRAGMENT_NODE);

	return true;
}

JSPropertySpec node_static_props[] = {
	JS_PSG("ELEMENT_NODE", node_static_get_property_ELEMENT_NODE, JSPROP_PERMANENT),
	JS_PSG("ATTRIBUTE_NODE", node_static_get_property_ATTRIBUTE_NODE, JSPROP_PERMANENT),
	JS_PSG("TEXT_NODE", node_static_get_property_TEXT_NODE, JSPROP_PERMANENT),
	JS_PSG("CDATA_SECTION_NODE", node_static_get_property_CDATA_SECTION_NODE, JSPROP_PERMANENT),
	JS_PSG("PROCESSING_INSTRUCTION_NODE", node_static_get_property_PROCESSING_INSTRUCTION_NODE, JSPROP_PERMANENT),
	JS_PSG("COMMENT_NODE", node_static_get_property_COMMENT_NODE, JSPROP_PERMANENT),
	JS_PSG("DOCUMENT_NODE", node_static_get_property_DOCUMENT_NODE, JSPROP_PERMANENT),
	JS_PSG("DOCUMENT_TYPE_NODE", node_static_get_property_DOCUMENT_TYPE_NODE, JSPROP_PERMANENT),
	JS_PSG("DOCUMENT_FRAGMENT_NODE", node_static_get_property_DOCUMENT_FRAGMENT_NODE, JSPROP_PERMANENT),
	JS_PS_END
};
