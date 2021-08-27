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


/*** common code ***/

static void bookmark_finalize(JSFreeOp *op, JSObject *obj);
static bool bookmark_folder_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

static JSClassOps bookmark_ops = {
	nullptr, nullptr,
	nullptr, nullptr,
	nullptr, nullptr, nullptr, bookmark_finalize,
};

static const JSClass bookmark_class = {
	"bookmark",
	JSCLASS_HAS_PRIVATE,	/* struct bookmark * */
	&bookmark_ops
};

static JSClassOps bookmark_folder_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
};

static const JSClass bookmark_folder_class = {
	"bookmark_folder",
	JSCLASS_HAS_PRIVATE,	/* struct bookmark * */
	&bookmark_folder_ops
};

static JSObject *
smjs_get_bookmark_generic_object(struct bookmark *bookmark, JSClass *clasp)
{
	JSObject *jsobj;

	assert(clasp == &bookmark_class || clasp == &bookmark_folder_class);
	if_assert_failed return NULL;

	jsobj = JS_NewObject(smjs_ctx, clasp);
	if (!jsobj) return NULL;

	if (!bookmark) return jsobj;

	JS_SetPrivate(jsobj, bookmark); /* to @bookmark_class or @bookmark_folder_class */
	object_lock(bookmark);

	return jsobj;
};

/* @bookmark_class.finalize, @bookmark_folder_class.finalize */
static void
bookmark_finalize(JSFreeOp *op, JSObject *obj)
{
	struct bookmark *bookmark;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_class, NULL)
	    || JS_InstanceOf(ctx, obj, (JSClass *) &bookmark_folder_class, NULL));
	if_assert_failed return;
#endif

	bookmark = JS_GetPrivate(obj); /* from @bookmark_class or @bookmark_folder_class */

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

static bool bookmark_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool bookmark_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool bookmark_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool bookmark_set_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool bookmark_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp);

static const JSPropertySpec bookmark_props[] = {
	JS_PSGS("title", bookmark_get_property_title, bookmark_set_property_title, JSPROP_ENUMERATE),
	JS_PSGS( "url", bookmark_get_property_url, bookmark_set_property_url, JSPROP_ENUMERATE),
	JS_PSG("children", bookmark_get_property_children, JSPROP_ENUMERATE),
	JS_PS_END
};

static JSObject *smjs_get_bookmark_folder_object(struct bookmark *bookmark);

/** Convert a string retrieved from struct bookmark to a jsval.
 *
 * @return true if successful.  On error, report the error and
 * return false.  */
static bool
bookmark_string_to_jsval(JSContext *ctx, const char *str, JS::Value *vp)
{
	JSString *jsstr = utf8_to_jsstring(ctx, str, -1);

	if (jsstr == NULL)
		return false;
	*vp = JS::StringValue(jsstr);
	return true;
}

/** Convert a JS::Value to a string and store it in struct bookmark.
 *
 * @param ctx
 *   Context for memory allocations and error reports.
 * @param val
 *   The @c JS::Value that should be converted.
 * @param[in,out] result
 *   A string allocated with mem_alloc().
 *   On success, this function frees the original string, if any.
 *
 * @return true if successful.  On error, report the error to
 * SpiderMonkey and return false.  */
static bool
jsval_to_bookmark_string(JSContext *ctx, JS::HandleValue val, char **result)
{
	char *str = jsval_to_string(ctx, val);

	if (str == NULL) {
		return false;
	}

	mem_free_set(result, str);
	return true;
}

static bool
bookmark_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct bookmark *bookmark;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_class, NULL))
		return false;

	bookmark = JS_GetInstancePrivate(ctx, hobj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return false;

	JSString *jsstr = utf8_to_jsstring(ctx, bookmark->title, -1);

	if (!jsstr) {
		return false;
	}
	args.rval().setString(jsstr);
	return true;
}

static bool
bookmark_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct bookmark *bookmark;
	char *title = NULL;
	char *url = NULL;
	int ok;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_class, NULL))
		return false;

	bookmark = JS_GetInstancePrivate(ctx, hobj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return false;

	if (!jsval_to_bookmark_string(ctx, args[0], &title))
		return false;

	ok = update_bookmark(bookmark, get_cp_index("UTF-8"), title, url);
	mem_free_if(title);
	mem_free_if(url);
	return ok ? true : false;
}

static bool
bookmark_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct bookmark *bookmark;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_class, NULL))
		return false;

	bookmark = JS_GetInstancePrivate(ctx, hobj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return false;

	JSString *jsstr = utf8_to_jsstring(ctx, bookmark->url, -1);

	if (!jsstr) {
		return false;
	}
	args.rval().setString(jsstr);
	return true;
}

static bool
bookmark_set_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct bookmark *bookmark;
	char *title = NULL;
	char *url = NULL;
	int ok;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_class, NULL))
		return false;

	bookmark = JS_GetInstancePrivate(ctx, hobj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return false;

	if (!jsval_to_bookmark_string(ctx, args[0], &url))
		return false;

	ok = update_bookmark(bookmark, get_cp_index("UTF-8"), title, url);
	mem_free_if(title);
	mem_free_if(url);
	return ok ? true : false;
}

static bool
bookmark_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct bookmark *bookmark;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_class, NULL))
		return false;

	bookmark = JS_GetInstancePrivate(ctx, hobj,
					 (JSClass *) &bookmark_class, NULL);

	if (!bookmark) return false;

	args.rval().setObject(*smjs_get_bookmark_folder_object(bookmark));

	return true;
}


static JSObject *
smjs_get_bookmark_object(struct bookmark *bookmark)
{
	JSObject *jsobj;

	jsobj = smjs_get_bookmark_generic_object(bookmark,
	                                         (JSClass *) &bookmark_class);

	JS::RootedObject r_jsobj(smjs_ctx, jsobj);
	if (jsobj
	     && true == JS_DefineProperties(smjs_ctx, r_jsobj,
	                                     (JSPropertySpec *) bookmark_props))
		return jsobj;

	return NULL;
}


/*** bookmark folder object ***/

/* @bookmark_folder_class.getProperty */
static bool
bookmark_folder_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct bookmark *bookmark;
	struct bookmark *folder;
	JS::Value val;
	JS::RootedValue title_jsval(ctx, val);
	char *title = NULL;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &bookmark_folder_class, NULL))
		return false;

	folder = JS_GetInstancePrivate(ctx, hobj,
				       (JSClass *) &bookmark_folder_class, NULL);

	hvp.setNull();

	if (!JS_IdToValue(ctx, id, &title_jsval))
		return false;

	if (!jsval_to_bookmark_string(ctx, title_jsval, &title))
		return false;

	bookmark = get_bookmark_by_name(folder, title);
	if (bookmark) {
		hvp.setObject(*smjs_get_bookmark_object(bookmark));
	}

	mem_free(title);
	return true;
}


static JSObject *
smjs_get_bookmark_folder_object(struct bookmark *bookmark)
{
	return smjs_get_bookmark_generic_object(bookmark,
	                                   (JSClass *) &bookmark_folder_class);
}

void
smjs_init_bookmarks_interface(void)
{
	JS::Value val;
	struct JSObject *bookmarks_object;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	bookmarks_object = smjs_get_bookmark_folder_object(NULL);
	if (!bookmarks_object) return;

	JS::RootedValue rval(smjs_ctx, val);
	rval.setObject(*bookmarks_object);
	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS_SetProperty(smjs_ctx, r_smjs_elinks_object, "bookmarks", rval);
}
