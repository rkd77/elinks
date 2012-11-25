/* "elinks.bookmarks" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "ecmascript/spidermonkey-shared.h"
#include "intl/charsets.h"
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

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in bookmark[-1];
 * future versions of ELinks may change the numbers.  */
enum bookmark_prop {
	BOOKMARK_TITLE    = -1,
	BOOKMARK_URL      = -2,
	BOOKMARK_CHILDREN = -3,
};

static const JSPropertySpec bookmark_props[] = {
	{ "title",    BOOKMARK_TITLE,    JSPROP_ENUMERATE },
	{ "url",      BOOKMARK_URL,      JSPROP_ENUMERATE },
	{ "children", BOOKMARK_CHILDREN, JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

static JSObject *smjs_get_bookmark_folder_object(struct bookmark *bookmark);

/** Convert a string retrieved from struct bookmark to a jsval.
 *
 * @return JS_TRUE if successful.  On error, report the error and
 * return JS_FALSE.  */
static JSBool
bookmark_string_to_jsval(JSContext *ctx, const unsigned char *str, jsval *vp)
{
	JSString *jsstr = utf8_to_jsstring(ctx, str, -1);

	if (jsstr == NULL)
		return JS_FALSE;
	*vp = STRING_TO_JSVAL(jsstr);
	return JS_TRUE;
}

/** Convert a jsval to a string and store it in struct bookmark.
 *
 * @param ctx
 *   Context for memory allocations and error reports.
 * @param val
 *   The @c jsval that should be converted.
 * @param[in,out] result
 *   A string allocated with mem_alloc().
 *   On success, this function frees the original string, if any.
 *
 * @return JS_TRUE if successful.  On error, report the error to
 * SpiderMonkey and return JS_FALSE.  */
static JSBool
jsval_to_bookmark_string(JSContext *ctx, jsval val, unsigned char **result)
{
	JSString *jsstr = NULL;
	unsigned char *str;

	/* JS_ValueToString constructs a new string if val is not
	 * already a string.  Protect the new string from the garbage
	 * collector, which jsstring_to_utf8() may trigger.
	 *
	 * Actually, SpiderMonkey 1.8.5 does not require this
	 * JS_AddNamedStringRoot call because it conservatively scans
	 * the C stack for GC roots.  Do the call anyway, because:
	 * 1. Omitting the call would require somehow ensuring that the
	 *    C compiler won't reuse the stack location too early.
	 *    (See template class js::Anchor in <jsapi.h>.)
	 * 2. Later versions of SpiderMonkey are switching back to
	 *    precise GC rooting, with a C++-only API.
	 * 3. jsval_to_bookmark_string() does not seem speed-critical.  */
	if (!JS_AddNamedStringRoot(ctx, &jsstr, "jsval_to_bookmark_string"))
		return JS_FALSE;

	jsstr = JS_ValueToString(ctx, val);
	if (jsstr == NULL) {
		JS_RemoveStringRoot(ctx, &jsstr);
		return JS_FALSE;
	}

	str = jsstring_to_utf8(ctx, jsstr, NULL);
	if (str == NULL) {
		JS_RemoveStringRoot(ctx, &jsstr);
		return JS_FALSE;
	}

	JS_RemoveStringRoot(ctx, &jsstr);
	mem_free_set(result, str);
	return JS_TRUE;
}

/* @bookmark_class.getProperty */
static JSBool
bookmark_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
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

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case BOOKMARK_TITLE:
		return bookmark_string_to_jsval(ctx, bookmark->title, vp);
	case BOOKMARK_URL:
		return bookmark_string_to_jsval(ctx, bookmark->url, vp);
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
bookmark_set_property(JSContext *ctx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	struct bookmark *bookmark;
	unsigned char *title = NULL;
	unsigned char *url = NULL;
	int ok;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_class, NULL))
		return JS_FALSE;

	bookmark = JS_GetInstancePrivate(ctx, obj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return JS_FALSE;

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case BOOKMARK_TITLE:
		if (!jsval_to_bookmark_string(ctx, *vp, &title))
			return JS_FALSE;
		break;
	case BOOKMARK_URL:
		if (!jsval_to_bookmark_string(ctx, *vp, &url))
			return JS_FALSE;
		break;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		return JS_TRUE;
	}

	ok = update_bookmark(bookmark, get_cp_index("UTF-8"), title, url);
	mem_free_if(title);
	mem_free_if(url);
	return ok ? JS_TRUE : JS_FALSE;
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
bookmark_folder_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	struct bookmark *bookmark;
	struct bookmark *folder;
	jsval title_jsval = JSVAL_VOID;
	unsigned char *title = NULL;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_folder_class, NULL))
		return JS_FALSE;

	folder = JS_GetInstancePrivate(ctx, obj,
				       (JSClass *) &bookmark_folder_class, NULL);

	*vp = JSVAL_NULL;

	if (!JS_IdToValue(ctx, id, &title_jsval))
		return JS_FALSE;

	if (!jsval_to_bookmark_string(ctx, title_jsval, &title))
		return JS_FALSE;

	bookmark = get_bookmark_by_name(folder, title);
	if (bookmark) {
		*vp = OBJECT_TO_JSVAL(smjs_get_bookmark_object(bookmark));
	}

	mem_free(title);
	return JS_TRUE;
}

static const JSClass bookmark_folder_class = {
	"bookmark_folder",
	JSCLASS_HAS_PRIVATE,	/* struct bookmark * */
	JS_PropertyStub, JS_PropertyStub,
	bookmark_folder_get_property, JS_StrictPropertyStub,
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
