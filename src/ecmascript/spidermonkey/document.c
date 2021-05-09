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
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/element.h"
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

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

#include <iostream>

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

	struct session *ses;
        int index;
        struct location *loc;

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	vs = interpreter->vs;
	struct document *document;
        struct document_view *doc_view = interpreter->vs->doc_view;
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);
	if (str)
	{
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
static bool document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ "replace",		document_replace,	1 },
	{ "getElementById",	document_getElementById,	1 },
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

	struct string code;

	init_string(&code);

	if (argc >= 1)
	{
		for (int i=0;i<argc;++i) 
		{

			jshandle_value_to_char_string(&code,ctx,&args[i]);
		}
	
		if (newline) 
		{
			add_to_string(&code, "\n");
		}
	}
	//DBG("%s",code.source);

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
	cached = doc_view->document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (f && f->length)
	{
		if (document->ecmascript_counter==0)
		{
			add_fragment(cached,0,code.source,code.length);
		} else {
			add_fragment(cached,f->length,code.source,code.length);
		}
		document->ecmascript_counter++;
	}



#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	done_string(&code);
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

void
string_replace(struct string *res, struct string *inp, struct string *what, struct string *repl)
{
	struct string tmp;
	struct string tmp2;
	char *head;
	char *found;
	char *ins;
	char *tmp_cnt;
	
	init_string(&tmp);
	init_string(&tmp2);

	add_string_to_string(&tmp, inp);
	
	head = tmp.source;
	int  count = 0;
	ins = head;
	if (what->length==0) 
	{
		add_string_to_string(res, inp); 
		return; 
	}

	// count occurence of string in input
	for (count = 0; tmp_cnt = strstr(ins, what->source); ++count) 
	{
		ins = tmp_cnt + what->length;
	}
	
	for (int i=0;i<count;i++) {
		// find occurence of string
		found=strstr(head,what->source);
		// count chars before and after occurence
		int bf_len=found-tmp.source;
		int af_len=tmp.length-bf_len-what->length;
		// move head by what
		found+=what->length;
		// join the before, needle and after to res
		add_bytes_to_string(&tmp2,tmp.source,bf_len);
		add_bytes_to_string(&tmp2,repl->source,repl->length);
		add_bytes_to_string(&tmp2,found,af_len);
		// clear tmp string and tmp2 string
		done_string(&tmp);
		init_string(&tmp);
		add_string_to_string(&tmp, &tmp2);
		done_string(&tmp2);
		init_string(&tmp2);
		//printf("TMP: %s |\n",tmp.source);
		head = tmp.source;
	}
	add_string_to_string(res, &tmp);

	done_string(&tmp);
	done_string(&tmp2);

}

/* @document_funcs{"replace"} */
static bool
document_replace(JSContext *ctx, unsigned int argc, JS::Value *vp)
{

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

	struct string needle;
	struct string heystack;

	init_string(&needle);
	init_string(&heystack);

	jshandle_value_to_char_string(&needle, ctx, &args[0]);
	jshandle_value_to_char_string(&heystack, ctx, &args[1]);

	//DBG("doc replace %s %s\n", needle.source, heystack.source);

	int nu_len=0;
	int fd_len=0;
	unsigned char *nu;
	struct cache_entry *cached = doc_view->document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (f && f->length)
	{
		fd_len=f->length;

		struct string f_data;
		init_string(&f_data);
		add_to_string(&f_data,f->data);

		struct string nu_str;
		init_string(&nu_str);
		string_replace(&nu_str,&f_data,&needle,&heystack);
		nu_len=nu_str.length;
		delete_entry_content(cached);
		/* This is very ugly, indeed. And Yes fd_len isn't 
		 * logically correct. But using nu_len will cause
		 * the document to render improperly.
		 * TBD: somehow better rerender the document 
		 * now it's places on the session level in doc_loading_callback */
		int ret = add_fragment(cached,0,nu_str.source,fd_len);
		normalize_cache_entry(cached,nu_len);
		document->ecmascript_counter++;
		//DBG("doc replace %s %s\n", needle.source, heystack.source);
	}

	done_string(&needle);
	done_string(&heystack);

	args.rval().setBoolean(true);

	return(true);
}

static void *
document_parse(struct document *document)
{
	struct cache_entry *cached = document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (!f || !f->length) {
		return NULL;
	}

	struct string str;
	init_string(&str);

	add_bytes_to_string(&str, f->data, f->length);

	 // Parse HTML and create a DOM tree
	xmlDoc* doc = htmlReadDoc((xmlChar*)str.source, NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlNode* r = xmlDocGetRootElement(doc);
	xmlpp::Element* root = new xmlpp::Element(r);
	done_string(&str);

	return (void *)root;
}


static bool
document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Element* root = (xmlpp::Element *)document->dom;

	struct string idstr;

	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string id = idstr.source;

	std::string xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

	auto elements = root->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto node = elements[0];
	JSObject *elem = getElement(ctx, node);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}
