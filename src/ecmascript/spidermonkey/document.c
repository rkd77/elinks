/* The SpiderMonkey document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/util.h"
#include <jsfriendapi.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "document/html/renderer.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/window.h"
#include "intl/gettext/libintl.h"
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


static bool document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

JSClassOps document_ops = {
	JS_PropertyStub, nullptr,
	document_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};


/* Each @document_class object must have a @window_class parent.  */
JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	&document_ops
};

#ifdef CONFIG_COOKIES
static bool
document_get_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct string *cookies;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static unsigned char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);
		args.rval().setString(JS_NewStringCopyZ(ctx, cookiestr));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	}

	return true;
}

static bool
document_set_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct string *cookies;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	set_cookie(vs->uri, JS_EncodeString(ctx, args[0].toString()));

	return true;
}

#endif

static bool
document_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	vs = interpreter->vs;
	struct document *document;
        struct document_view *doc_view = interpreter->vs->doc_view;
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);
	if (str) {
		args.rval().setString(JS_NewStringCopyZ(ctx, str));
		mem_free(str);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
document_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

	return true;
}


static bool
document_get_property_referrer(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		args.rval().setUndefined();
		break;

	case REFERER_FAKE:
		args.rval().setString(JS_NewStringCopyZ(ctx, get_opt_str("protocol.http.referer.fake", NULL)));
		break;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
		if (ses->referrer) {
			char *str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				args.rval().setString(JS_NewStringCopyZ(ctx, str));
				mem_free(str);
			} else {
				args.rval().setUndefined();
			}
		}
		break;

	case REFERER_SAME_URL:
		char *str = get_uri_string(document->uri, URI_HTTP_REFERRER);

		if (str) {
			args.rval().setString(JS_NewStringCopyZ(ctx, str));
			mem_free(str);
		} else {
			args.rval().setUndefined();
		}
		break;
	}

	return true;
}


static bool
document_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	args.rval().setString(JS_NewStringCopyZ(ctx, document->title));

	return true;
}

static bool
document_set_property_title(JSContext *ctx, int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);


	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs || !vs->doc_view) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	mem_free_set(&document->title, stracpy(JS_EncodeString(ctx, args[0].toString())));
	print_screen_status(doc_view->session);

	return true;
}

static bool
document_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);

	if (str) {
		args.rval().setString(JS_NewStringCopyZ(ctx, str));
		mem_free(str);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
document_set_property_url(JSContext *ctx, int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

	return true;
}


/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
JSPropertySpec document_props[] = {
#ifdef CONFIG_COOKIES
	JS_PSGS("cookie", document_get_property_cookie, document_set_property_cookie, JSPROP_ENUMERATE),
#endif
	JS_PSGS("location",	document_get_property_location, document_set_property_location, JSPROP_ENUMERATE),
	JS_PSG("referrer",	document_get_property_referrer, JSPROP_ENUMERATE),
	JS_PSGS("title",	document_get_property_title, document_set_property_title, JSPROP_ENUMERATE), /* TODO: Charset? */
	JS_PSGS("url",	document_get_property_url, document_set_property_url, JSPROP_ENUMERATE),
	JS_PS_END
};


/* @document_class.getProperty */
static bool
document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JS::RootedObject parent_win(ctx);	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form *form;
	char *string;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);


	JSClass* classPtr = JS_GetClass(hobj);

	if (classPtr != &document_class)
		return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (!JSID_IS_STRING(hid)) {
		return true;
	}
	string = jsid_to_string(ctx, hid);

	foreach (form, document->forms) {
		if (!form->name || c_strcasecmp(string, form->name))
			continue;

		hvp.setObject(*get_form_object(ctx, hobj, find_form_view(doc_view, form)));
		break;
	}

	return true;
}

static bool document_write(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_replace(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ "replace",		document_replace,	1 },
	{ NULL }
};

static bool
document_write_do(JSContext *ctx, unsigned int argc, JS::Value *rval, int newline)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	JS::Value val;
	struct string *ret = interpreter->ret;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

        unsigned char *code;

	//if (argc >= 1 && ret) {
	//	int i = 0;

	//	for (; i < argc; ++i) {
	//		char *code = jsval_to_string(ctx, args[i]);

	//		add_to_string(ret, code);
	//	}
	//
	//	if (newline) {
	//		add_char_to_string(ret, '\n');
	//	}
	//}

	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

        struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document;
	document = doc_view->document;
        struct cache_entry *cached = doc_view->document->cached;
        struct fragment *f = cached ? cached->frag.next : NULL;
        cached = doc_view->document->cached;
 	f = get_cache_fragment(cached);
	struct string buffer = INIT_STRING("", 0);
        if (f && f->length) {
		char *code = jsval_to_string(ctx, args[0]);
		int code_len=strlen(code);
		if (newline==1) {
			code_len++;
			unsigned char *codetmp=malloc(code_len);
			sprintf(codetmp,"%s\n",code);
			code=realloc(code,code_len);
			sprintf(code,"%s",codetmp);
		} 
 		if (document->ecmascript_counter==0) {
			add_fragment(cached,0,code,code_len);
		} else {
			add_fragment(cached,f->length,code,code_len);
		}
		struct fragment *frag;
		frag = get_cache_fragment(cached);
		if (frag) {
			buffer.source = frag->data; 
			buffer.length = frag->length;
		}
		// if the document is empty and has no body then it's required
		// to setup data to program to function correctly
		if (!document->data) {
			init_document(cached,&document->options);
		} else {
			// if the document is not empty the written doc is longer than displayed
			// this fixes the previous on-screen document display data
			document->data->length=0;
 		}
		garbage_collection(1);
		render_html_document(cached, document, &buffer); 
		document->ecmascript_counter++;
	}
  

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	args.rval().setBoolean(false);

	return true;
}

/* @document_funcs{"write"} */
static bool
document_write(JSContext *ctx, unsigned int argc, JS::Value *rval)
{

	return document_write_do(ctx, argc, rval, 0);
}

/* @document_funcs{"writeln"} */
static bool
document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	return document_write_do(ctx, argc, rval, 1);
}

// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";

    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    result = tmp = malloc(strlen(orig) + ( (len_with - len_rep) * count) + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    int xocc = count;
    while (count--) {
        //printf("%d",xocc);
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    //sprintf(tmp,'%s',orig);
    return(result);
}


/* @document_funcs{"replace"} */
static bool
document_replace(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	//jsval val;
	//struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
        struct document_view *doc_view = interpreter->vs->doc_view;
        struct session *ses = doc_view->session;
        struct terminal *term = ses->tab->term;
	struct string *ret = interpreter->ret;
	struct document *document;
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	document = doc_view->document;
        
        if (argc != 2) {
	       args.rval().setBoolean(false);
	       return(true);
        }


        unsigned char *needle = JS_EncodeString(ctx, args[0].toString());
        unsigned char *heystack = JS_EncodeString(ctx, args[1].toString());
	//DBG("doc replace %s %s\n", needle, heystack);
        
	struct string buffer = INIT_STRING("", 0);
        int nu_len=0;
        int fd_len=0;
        unsigned char *nu;
        struct cache_entry *cached = doc_view->document->cached;
        struct fragment *f = cached ? cached->frag.next : NULL;
        cached = doc_view->document->cached;
 	f = get_cache_fragment(cached);
        if (f && f->length) {
        	fd_len=strlen(f->data);
		nu=str_replace(f->data,needle,heystack);
		nu_len=strlen(nu);
		buffer.source=nu;
		buffer.length=nu_len;
		// THIS IS UGLY BUT nu AND fd_len IS CORRECT
		int ret=add_fragment(cached,0,nu,fd_len);
		document->data->length=0;
		clear_terminal(term);
		render_html_document(cached,document,&buffer);
		normalize_cache_entry(cached,strlen(nu));
		garbage_collection(1);
 	}

	args.rval().setBoolean(true);

	return(true);
}

