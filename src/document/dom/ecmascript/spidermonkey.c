/* SpiderMonkey's DOM implementation */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Attr.h"
#include "document/dom/ecmascript/spidermonkey/CDATASection.h"
#include "document/dom/ecmascript/spidermonkey/CharacterData.h"
#include "document/dom/ecmascript/spidermonkey/Comment.h"
#include "document/dom/ecmascript/spidermonkey/Document.h"
#include "document/dom/ecmascript/spidermonkey/DocumentFragment.h"
#include "document/dom/ecmascript/spidermonkey/DocumentType.h"
#include "document/dom/ecmascript/spidermonkey/DOMConfiguration.h"
#include "document/dom/ecmascript/spidermonkey/DOMError.h"
#include "document/dom/ecmascript/spidermonkey/DOMErrorHandler.h"
#include "document/dom/ecmascript/spidermonkey/DOMException.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementation.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationList.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationRegistry.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationSource.h"
#include "document/dom/ecmascript/spidermonkey/DOMLocator.h"
#include "document/dom/ecmascript/spidermonkey/DOMStringList.h"
#include "document/dom/ecmascript/spidermonkey/Element.h"
#include "document/dom/ecmascript/spidermonkey/Entity.h"
#include "document/dom/ecmascript/spidermonkey/EntityReference.h"
#include "document/dom/ecmascript/spidermonkey/NamedNodeMap.h"
#include "document/dom/ecmascript/spidermonkey/NameList.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/Notation.h"
#include "document/dom/ecmascript/spidermonkey/ProcessingInstruction.h"
#include "document/dom/ecmascript/spidermonkey/Text.h"
#include "document/dom/ecmascript/spidermonkey/TypeInfo.h"
#include "document/dom/ecmascript/spidermonkey/UserDataHandler.h"
#include "dom/node.h"

void
done_dom_node_ecmascript_obj(struct dom_node *node)
{
	struct dom_node *root = get_dom_root_node(node);
	JSContext *ctx = root->data.document.ecmascript_ctx;
	JSObject *obj = node->ecmascript_obj;

	assert(ctx && obj && JS_GetPrivate(ctx, obj) == node);
	JS_SetPrivate(ctx, obj, NULL);
	node->ecmascript_obj = NULL;
}

JSObject *
make_new_object(JSContext *ctx, struct dom_node *node)
{
#if 0
	JSObject *obj = create_new_object[node->data.element.type](ctx);

	if (obj) {
		JS_SetPrivate(ctx, obj, node);
		node->ecmascript_obj = obj;
	}
	return obj;
#endif
	return NULL;
}

void
dom_add_attribute(struct dom_node *root, struct dom_node *node, struct dom_string *attr, struct dom_string *value)
{
	JSContext *ctx = root->data.document.ecmascript_ctx;
	JSObject *obj = node->ecmascript_obj;
	JSString *string;
	jsval vp;
	unsigned char tmp;

	if (!obj) {
		obj = make_new_object(ctx, node);
		if (!obj) return;
	}
	if (value) {
		string = JS_NewString(ctx, value->string, value->length);
		vp = STRING_TO_JSVAL(string);
	} else {
		vp = JSVAL_NULL;
	}
	tmp = attr->length;
	attr->string[attr->length] = '\0';
	JS_SetProperty(ctx, obj, attr->string, &vp);
	attr->string[attr->length] = tmp;
}
