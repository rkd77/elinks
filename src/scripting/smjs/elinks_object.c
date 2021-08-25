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
#include "intl/libintl.h"
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
static bool
elinks_alert(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);

	char *string;
	struct terminal *term;

	if (argc != 1)
		return true;

	string = JS_EncodeString(ctx, args[0].toString());

	if (!*string)
		return true;

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

	args.rval().setUndefined();

	return true;
}

/* @elinks_funcs{"execute"} */
static bool
elinks_execute(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);

	char *string;

	if (argc != 1)
		return true;

	string = JS_EncodeString(ctx, args[0].toString());
	if (!*string)
		return true;

	exec_on_terminal(smjs_ses->tab->term, string, "", TERM_EXEC_BG);

	args.rval().setUndefined();
	return true;
}

enum elinks_prop {
	ELINKS_HOME,
	ELINKS_LOCATION,
	ELINKS_SESSION,
};

static bool elinks_get_property_home(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool elinks_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool elinks_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool elinks_get_property_session(JSContext *ctx, unsigned int argc, JS::Value *vp);
static const JSPropertySpec elinks_props[] = {
	JS_PSG("home", elinks_get_property_home, JSPROP_ENUMERATE),
	JS_PSGS("location", elinks_get_property_location, elinks_set_property_location, JSPROP_ENUMERATE),
	JS_PSG("session", elinks_get_property_session, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool elinks_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool elinks_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

static const JSClassOps elinks_ops = {
	nullptr, nullptr,
	elinks_get_property, elinks_set_property,
	nullptr, nullptr, nullptr, nullptr
};

static const JSClass elinks_class = {
	"elinks",
	0,
	&elinks_ops
};


/* @elinks_class.getProperty */
static bool
elinks_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return false here, the object's methods and
		 * user-added properties do not work. */
		return true;
	}

	hvp.setUndefined();

	switch (JSID_TO_INT(id)) {
	case ELINKS_HOME:
		hvp.setString(JS_NewStringCopyZ(smjs_ctx, elinks_home));

		return true;
	case ELINKS_LOCATION: {
		struct uri *uri;

		if (!smjs_ses) return false;

		uri = have_location(smjs_ses) ? cur_loc(smjs_ses)->vs.uri
					      : smjs_ses->loading_uri;

		hvp.setString(JS_NewStringCopyZ(smjs_ctx,
		                        uri ? (const char *) struri(uri) : ""));

		return true;
	}
	case ELINKS_SESSION: {
		JSObject *jsobj;

		if (!smjs_ses) return false;

		jsobj = smjs_get_session_object(smjs_ses);
		if (!jsobj) return false;

		hvp.setObject(*jsobj);

		return true;
	}
	default:
		INTERNAL("Invalid ID %d in elinks_get_property().",
		         JSID_TO_INT(id));
	}

	return false;
}

static bool
elinks_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return false here, the object's methods and
		 * user-added properties do not work. */
		return true;
	}

	switch (JSID_TO_INT(id)) {
	case ELINKS_LOCATION: {
	       JSString *jsstr;
	       char *url;

	       if (!smjs_ses) return false;

	       jsstr = hvp.toString();
	       if (!jsstr) return false;

	       url = JS_EncodeString(smjs_ctx, jsstr);
	       if (!url) return false;

	       goto_url(smjs_ses, url);

	       return true;
	}
	default:
		INTERNAL("Invalid ID %d in elinks_set_property().",
		         JSID_TO_INT(id));
	}

	return false;
}


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
 * store the return value in rval, and return true. Else return false. */
bool
smjs_invoke_elinks_object_method(char *method, int argc, JS::Value *argv, JS::MutableHandleValue rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, argv);

	assert(smjs_ctx);
	assert(smjs_elinks_object);
	assert(argv);

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);
	JS::RootedValue fun(smjs_ctx);

	if (false == JS_GetProperty(smjs_ctx, r_smjs_elinks_object,
	                               method, &fun)) {
		return false;
	}

	return JS_CallFunctionValue(smjs_ctx, r_smjs_elinks_object, fun, args, rval);
}

static bool
elinks_get_property_home(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, elinks_home));

	return true;
}

static bool
elinks_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct uri *uri;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	if (!smjs_ses) return false;

	uri = have_location(smjs_ses) ? cur_loc(smjs_ses)->vs.uri : smjs_ses->loading_uri;
	args.rval().setString(JS_NewStringCopyZ(smjs_ctx, uri ? (const char *) struri(uri) : ""));

	return true;
}

static bool
elinks_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSString *jsstr;
	char *url;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	if (!smjs_ses) return false;

	jsstr = args[0].toString();
	if (!jsstr) return false;

	url = JS_EncodeString(smjs_ctx, jsstr);
	if (!url) return false;

	goto_url(smjs_ses, url);

	return true;
}

static bool
elinks_get_property_session(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSObject *jsobj;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &elinks_class, NULL))
		return false;

	if (!smjs_ses) return false;

	jsobj = smjs_get_session_object(smjs_ses);
	if (!jsobj) return false;

	args.rval().setObject(*jsobj);;

	return true;
}
