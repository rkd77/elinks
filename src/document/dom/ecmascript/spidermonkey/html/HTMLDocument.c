#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Document.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDocument.h"

static JSBool
HTMLDocument_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DOCUMENT_TITLE:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_REFERER:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_DOMAIN:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_URL:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_BODY:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_IMAGES:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_APPLETS:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_LINKS:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_FORMS:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_ANCHORS:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_COOKIE:
		/* Write me! */
		break;
	default:
		return Document_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLDocument_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DOCUMENT_TITLE:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_BODY:
		/* Write me! */
		break;
	case JSP_HTML_DOCUMENT_COOKIE:
		/* Write me! */
		break;
	default:
		return Document_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLDocument_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_close(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_writeln(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_getElementsByName(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLDocument_props[] = {
	{ "title",	JSP_HTML_DOCUMENT_TITLE,	JSPROP_ENUMERATE },
	{ "referer",	JSP_HTML_DOCUMENT_REFERER,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "domain",	JSP_HTML_DOCUMENT_DOMAIN,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "URL",	JSP_HTML_DOCUMENT_URL,		JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "body",	JSP_HTML_DOCUMENT_BODY,		JSPROP_ENUMERATE },
	{ "images",	JSP_HTML_DOCUMENT_IMAGES,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "applets",	JSP_HTML_DOCUMENT_APPLETS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "links",	JSP_HTML_DOCUMENT_LINKS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "forms",	JSP_HTML_DOCUMENT_FORMS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "anchors",	JSP_HTML_DOCUMENT_ANCHORS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "cookie",	JSP_HTML_DOCUMENT_COOKIE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLDocument_funcs[] = {
	{ "open",		HTMLDocument_open,		0 },
	{ "close",		HTMLDocument_close,		0 },
	{ "write",		HTMLDocument_write,		1 },
	{ "writeln",		HTMLDocument_writeln,		1 },
	{ "getElementsByName",	HTMLDocument_getElementsByName,	1 },
	{ NULL }
};

const JSClass HTMLDocument_class = {
	"HTMLDocument",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLDocument_getProperty, HTMLDocument_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

