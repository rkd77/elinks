#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLinkElement.h"

static JSBool
HTMLLinkElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LINK_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_HREFLANG:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_MEDIA:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_REL:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_REV:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_TARGET:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLinkElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LINK_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_HREFLANG:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_MEDIA:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_REL:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_REV:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_TARGET:
		/* Write me! */
		break;
	case JSP_HTML_LINK_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLLinkElement_props[] = {
	{ "disabled",	JSP_HTML_LINK_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_LINK_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_LINK_ELEMENT_HREF,	JSPROP_ENUMERATE },
	{ "hreflang",	JSP_HTML_LINK_ELEMENT_HREFLANG,	JSPROP_ENUMERATE },
	{ "media",	JSP_HTML_LINK_ELEMENT_MEDIA,	JSPROP_ENUMERATE },
	{ "rel",	JSP_HTML_LINK_ELEMENT_REL,	JSPROP_ENUMERATE },
	{ "rev",	JSP_HTML_LINK_ELEMENT_REV,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_LINK_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_LINK_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLLinkElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLLinkElement_class = {
	"HTMLLinkElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLLinkElement_getProperty, HTMLLinkElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

