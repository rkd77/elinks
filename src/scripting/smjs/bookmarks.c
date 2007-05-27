/* "elinks.bookmarks" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "ecmascript/spidermonkey/util.h"
#include "main/event.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "util/memory.h"


static const JSClass bookmark_class, bookmark_folder_class; /* defined below */


/*** common code ***/

static JSObject *
smjs_get_bookmark_generic_object(struct bookmark *bookmark, JSClass *clasp)
{
	JSObject *jsobj;

	assert(clasp == &bookmark_class || clasp == &bookmark_folder_class);
	if_assert_failed return NULL;

	jsobj = JS_NewObject(smjs_ctx, clasp, NULL, NULL);
	if (!jsobj) return NULL;

	if (!bookmark) return jsobj;

	if (JS_TRUE == JS_SetPrivate(smjs_ctx, jsobj, bookmark)) { /* to @bookmark_class or @bookmark_folder_class */
		object_lock(bookmark);

		return jsobj;
	}

	return NULL;
};

/* @bookmark_class.finalize, @bookmark_folder_class.finalize */
static void
bookmark_finalize(JSContext *ctx, JSObject *obj)
{
	struct bookmark *bookmark;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_class, NULL)
	    || JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_folder_class, NULL));
	if_assert_failed return;

	bookmark = JS_GetPrivate(ctx, obj); /* from @bookmark_class or @bookmark_folder_class */

	if (bookmark) object_unlock(bookmark);
}


/*** bookmark object ***/

enum bookmark_prop {
	BOOKMARK_TITLE,
	BOOKMARK_URL,
	BOOKMARK_CHILDREN,
};

static const JSPropertySpec bookmark_props[] = {
	{ "title",    BOOKMARK_TITLE,    JSPROP_ENUMERATE },
	{ "url",      BOOKMARK_URL,      JSPROP_ENUMERATE },
	{ "children", BOOKMARK_CHILDREN, JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

static JSObject *smjs_get_bookmark_folder_object(struct bookmark *bookmark);

/* @bookmark_class.getProperty */
static JSBool
bookmark_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct bookmark *bookmark;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_class, NULL))
		return JS_FALSE;

	bookmark = JS_GetInstancePrivate(ctx, obj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return JS_FALSE;

	undef_to_jsval(ctx, vp);

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case BOOKMARK_TITLE:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                bookmark->title));

		return JS_TRUE;
	case BOOKMARK_URL:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
	                                                bookmark->url));

		return JS_TRUE;
	case BOOKMARK_CHILDREN:
		*vp = OBJECT_TO_JSVAL(smjs_get_bookmark_folder_object(bookmark));

		return JS_TRUE;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		return JS_TRUE;
	}
}

/* @bookmark_class.setProperty */
static JSBool
bookmark_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct bookmark *bookmark;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_class, NULL))
		return JS_FALSE;

	bookmark = JS_GetInstancePrivate(ctx, obj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return JS_FALSE;

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case BOOKMARK_TITLE: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&bookmark->title, stracpy(str));

		return JS_TRUE;
	}
	case BOOKMARK_URL: {
		JSString *jsstr = JS_ValueToString(smjs_ctx, *vp);
		unsigned char *str = JS_GetStringBytes(jsstr);

		mem_free_set(&bookmark->url, stracpy(str));

		return JS_TRUE;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		return JS_TRUE;
	}
}

static const JSClass bookmark_class = {
	"bookmark",
	JSCLASS_HAS_PRIVATE,	/* struct bookmark * */
	JS_PropertyStub, JS_PropertyStub,
	bookmark_get_property, bookmark_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, bookmark_finalize,
};

static JSObject *
smjs_get_bookmark_object(struct bookmark *bookmark)
{
	JSObject *jsobj;

	jsobj = smjs_get_bookmark_generic_object(bookmark,
	                                         (JSClass *) &bookmark_class);

	if (jsobj
	     && JS_TRUE == JS_DefineProperties(smjs_ctx, jsobj,
	                                     (JSPropertySpec *) bookmark_props))
		return jsobj;

	return NULL;
}


/*** bookmark folder object ***/

/* @bookmark_folder_class.getProperty */
static JSBool
bookmark_folder_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct bookmark *bookmark;
	struct bookmark *folder;
	unsigned char *title;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_folder_class, NULL))
		return JS_FALSE;

	folder = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &bookmark_folder_class, NULL);

	title = JS_GetStringBytes(JS_ValueToString(ctx, id));
	if (!title) {
		*vp = JSVAL_NULL;

		return JS_TRUE;
	}

	bookmark = get_bookmark_by_name(folder, title);
	if (!bookmark) {
		*vp = JSVAL_NULL;

		return JS_TRUE;
	}

	*vp = OBJECT_TO_JSVAL(smjs_get_bookmark_object(bookmark));

	return JS_TRUE;
}

static const JSClass bookmark_folder_class = {
	"bookmark_folder",
	JSCLASS_HAS_PRIVATE,	/* struct bookmark * */
	JS_PropertyStub, JS_PropertyStub,
	bookmark_folder_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, bookmark_finalize,
};

static JSObject *
smjs_get_bookmark_folder_object(struct bookmark *bookmark)
{
	return smjs_get_bookmark_generic_object(bookmark,
	                                   (JSClass *) &bookmark_folder_class);
}

void
smjs_init_bookmarks_interface(void)
{
	jsval val;
	struct JSObject *bookmarks_object;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	bookmarks_object = smjs_get_bookmark_folder_object(NULL);
	if (!bookmarks_object) return;

	val = OBJECT_TO_JSVAL(bookmarks_object);

	JS_SetProperty(smjs_ctx, smjs_elinks_object, "bookmarks", &val);
}
