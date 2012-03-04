/* The "elinks" object */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "config/home.h"
#include "ecmascript/spidermonkey-shared.h"
#include "ecmascript/spidermonkey/util.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "scripting/scripting.h"
#include "scripting/smjs/action_object.h"
#include "scripting/smjs/bookmarks.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/global_object.h"
#include "scripting/smjs/globhist.h"
#include "scripting/smjs/keybinding.h"
#include "scripting/smjs/load_uri.h"
#include "scripting/smjs/session_object.h"
#include "scripting/smjs/view_state_object.h"
#include "scripting/smjs/terminal_object.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"


/* @elinks_funcs{"alert"} */
static JSBool
elinks_alert(JSContext *ctx, uintN argc, jsval *rval)
{
	jsval val;
	jsval *argv = JS_ARGV(ctx, rval);
	unsigned char *string;
	struct terminal *term;

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	if (smjs_ses) {
		term = smjs_ses->tab->term;
	} else {
		term = get_default_terminal();
	}

	if (term) {
		info_box(term, MSGBOX_NO_TEXT_INTL,
			 N_("User script alert"), ALIGN_LEFT, string);
	} else {
		usrerror("%s", string);
		sleep(3);
	}

	undef_to_jsval(ctx, &val);
	JS_SET_RVAL(ctx, rval, val);

	return JS_TRUE;
}

/* @elinks_funcs{"execute"} */
static JSBool
elinks_execute(JSContext *ctx, uintN argc, jsval *rval)
{
	jsval val;
	jsval *argv = JS_ARGV(ctx, rval);
	unsigned char *string;

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	exec_on_terminal(smjs_ses->tab->term, string, "", TERM_EXEC_BG);

	undef_to_jsval(ctx, &val);
	JS_SET_RVAL(ctx, rval, val);
	return JS_TRUE;
}

enum elinks_prop {
	ELINKS_HOME,
	ELINKS_LOCATION,
	ELINKS_SESSION,
};

static const JSPropertySpec elinks_props[] = {
	{ "home",     ELINKS_HOME,     JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY },
	{ "location", ELINKS_LOCATION, JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{ "session",  ELINKS_SESSION,  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY },
	{ NULL }
};

static const JSClass elinks_class;

/* @elinks_class.getProperty */
static JSBool
elinks_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &elinks_class, NULL))
		return JS_FALSE;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return JS_FALSE here, the object's methods and
		 * user-added properties do not work. */
		return JS_TRUE;
	}

	undef_to_jsval(ctx, vp);

	switch (JSID_TO_INT(id)) {
	case ELINKS_HOME:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx, elinks_home));

		return JS_TRUE;
	case ELINKS_LOCATION: {
		struct uri *uri;

		if (!smjs_ses) return JS_FALSE;

		uri = have_location(smjs_ses) ? cur_loc(smjs_ses)->vs.uri
					      : smjs_ses->loading_uri;

		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                        uri ? (const char *) struri(uri) : ""));

		return JS_TRUE;
	}
	case ELINKS_SESSION: {
		JSObject *jsobj;

		if (!smjs_ses) return JS_FALSE;

		jsobj = smjs_get_session_object(smjs_ses);
		if (!jsobj) return JS_FALSE;

		object_to_jsval(ctx, vp, jsobj);

		return JS_TRUE;
	}
	default:
		INTERNAL("Invalid ID %d in elinks_get_property().",
		         JSID_TO_INT(id));
	}

	return JS_FALSE;
}

static JSBool
elinks_set_property(JSContext *ctx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &elinks_class, NULL))
		return JS_FALSE;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return JS_FALSE here, the object's methods and
		 * user-added properties do not work. */
		return JS_TRUE;
	}

	switch (JSID_TO_INT(id)) {
	case ELINKS_LOCATION: {
	       JSString *jsstr;
	       unsigned char *url;

	       if (!smjs_ses) return JS_FALSE;

	       jsstr = JS_ValueToString(smjs_ctx, *vp);
	       if (!jsstr) return JS_FALSE;

	       url = JS_EncodeString(smjs_ctx, jsstr);
	       if (!url) return JS_FALSE;

	       goto_url(smjs_ses, url);

	       return JS_TRUE;
	}
	default:
		INTERNAL("Invalid ID %d in elinks_set_property().",
		         JSID_TO_INT(id));
	}

	return JS_FALSE;
}

static const JSClass elinks_class = {
	"elinks",
	0,
	JS_PropertyStub, JS_PropertyStub,
	elinks_get_property, elinks_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static const spidermonkeyFunctionSpec elinks_funcs[] = {
	{ "alert",	elinks_alert,		1 },
	{ "execute",	elinks_execute,		1 },
	{ NULL }
};

static JSObject *
smjs_get_elinks_object(void)
{
	assert(smjs_ctx);
	assert(smjs_global_object);

	return spidermonkey_InitClass(smjs_ctx, smjs_global_object, NULL,
	                              (JSClass *) &elinks_class, NULL, 0,
	                              (JSPropertySpec *) elinks_props,
	                              elinks_funcs, NULL, NULL);
}

void
smjs_init_elinks_object(void)
{
	smjs_elinks_object = smjs_get_elinks_object(); /* TODO: check NULL */

	smjs_init_action_interface();
	smjs_init_bookmarks_interface();
	smjs_init_globhist_interface();
	smjs_init_keybinding_interface();
	smjs_init_load_uri_interface();
	smjs_init_view_state_interface();
	smjs_init_session_interface();
	smjs_init_terminal_interface();
}

/* If elinks.<method> is defined, call it with the given arguments,
 * store the return value in rval, and return JS_TRUE. Else return JS_FALSE. */
JSBool
smjs_invoke_elinks_object_method(unsigned char *method, jsval argv[], int argc,
                                 jsval *rval)
{
	assert(smjs_ctx);
	assert(smjs_elinks_object);
	assert(rval);
	assert(argv);

	if (JS_FALSE == JS_GetProperty(smjs_ctx, smjs_elinks_object,
	                               method, rval))
		return JS_FALSE;

	if (JSVAL_IS_VOID(*rval))
		return JS_FALSE;

	return JS_CallFunctionValue(smjs_ctx, smjs_elinks_object,
				    *rval, argc, argv, rval);
}
