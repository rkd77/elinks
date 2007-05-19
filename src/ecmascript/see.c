/* The SEE ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include <see/see.h>

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
#include "ecmascript/see.h"
#include "ecmascript/see/document.h"
#include "ecmascript/see/form.h"
#include "ecmascript/see/input.h"
#include "ecmascript/see/location.h"
#include "ecmascript/see/navigator.h"
#include "ecmascript/see/strings.h"
#include "ecmascript/see/unibar.h"
#include "ecmascript/see/window.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"


/*** Global methods */


/* TODO? Are there any which need to be implemented? */

static void
see_init(struct module *xxx)
{
	init_intern_strings();
	SEE_system.periodic = checktime;
}

static void
see_done(struct module *xxx)
{
}

void *
see_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = SEE_NEW(NULL, struct global_object);
	struct SEE_interpreter *interp = &g->interp;

	interpreter->backend_data = g;
	g->max_exec_time = get_opt_int("ecmascript.max_exec_time");
	g->exec_start = time(NULL);
	/* used by setTimeout */
	g->interpreter = interpreter;
	SEE_interpreter_init(interp);
	init_js_window_object(interpreter);
	init_js_menubar_object(interpreter);
	init_js_statusbar_object(interpreter);
	init_js_navigator_object(interpreter);
	init_js_history_object(interpreter);
	init_js_location_object(interpreter);
	init_js_document_object(interpreter);
	init_js_forms_object(interpreter);
	return interp;
}

void
see_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	interpreter->backend_data = NULL;
}

static void
see_report_error(struct ecmascript_interpreter *interpreter, struct SEE_value *e)
{
	SEE_try_context_t ctx;
	struct string msg;
	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct terminal *term;
	unsigned char *exception = NULL;

#if 0
	assert(g &&
	g->win &&
	g->win->vs &&
	g->win->vs->doc_view
	       && g->win->vs->doc_view->session && g->interp
	       && g->win->vs->doc_view->session->tab);
	if_assert_failed return;
#endif

	term = g->win->vs->doc_view->session->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(g->win->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting"))
		return;

	SEE_TRY(interp, ctx) {
		exception = see_value_to_unsigned_char(interp, e);
	}
	if (SEE_CAUGHT(ctx)) {
		mem_free_if(exception);
		return;
	}

	if (!exception || !init_string(&msg)) {
		mem_free_if(exception);
		return;
	}
	add_format_to_string(&msg, _("A script embedded in the current "
		"document raised the exception: %s", term), exception);
	mem_free(exception);
	info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER,
		 msg.source);
}

void
see_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{

	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct SEE_input *input = see_input_elinks(interp, code->source);
	SEE_try_context_t try_ctxt;
	struct SEE_value result;
	struct SEE_value *e;

	g->exec_start = time(NULL);
	g->ret = ret;
	SEE_TRY(interp, try_ctxt) {
		SEE_Global_eval(interp, input, &result);
	}

	SEE_INPUT_CLOSE(input);
	e = SEE_CAUGHT(try_ctxt);
	if (e)
		see_report_error(interpreter, e);
}


unsigned char *
see_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct SEE_input *input = see_input_elinks(interp, code->source);
	SEE_try_context_t try_ctxt;
	struct SEE_value result;
	struct SEE_value *e;
	/* 'volatile' qualifier prevents register allocation which fixes:
	 *  warning: variable 'xxx' might be clobbered by 'longjmp' or 'vfork'
	 */
	unsigned char *volatile string = NULL;

	g->exec_start = time(NULL);
	g->ret = NULL;
	SEE_TRY(interp, try_ctxt) {
		SEE_Global_eval(interp, input, &result);
		if (SEE_VALUE_GET_TYPE(&result) == SEE_STRING)
			string = see_value_to_unsigned_char(interp, &result);

	}
	SEE_INPUT_CLOSE(input);
	e = SEE_CAUGHT(try_ctxt);
	if (e) {
		see_report_error(interpreter, e);
		return NULL;
	}
	return string;
}

int
see_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct SEE_input *input = see_input_elinks(interp, code->source);
	struct SEE_object *fun;
	SEE_try_context_t try_ctxt;
	struct SEE_value result;
	struct SEE_value *e;
	/* 'volatile' qualifier prevents register allocation which fixes:
	 *  warning: variable 'xxx' might be clobbered by 'longjmp' or 'vfork'
	 */
	SEE_int32_t volatile res = 0;

	g->exec_start = time(NULL);
	g->ret = NULL;
	SEE_TRY(interp, try_ctxt) {
		fun = SEE_Function_new(interp, NULL, NULL, input);
		SEE_OBJECT_CALL(interp, fun, NULL, 0, NULL, &result);
		/* history.back() returns SEE_NULL */
		if (SEE_VALUE_GET_TYPE(&result) == SEE_NULL)
			res = 0;
		else
			res = SEE_ToInt32(interp, &result);
	}

	SEE_INPUT_CLOSE(input);
	e = SEE_CAUGHT(try_ctxt);
	if (e) {
		see_report_error(interpreter, e);
		return -1;
	}
	return res;
}

struct module see_module = struct_module(
	/* name: */		N_("SEE"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		see_init,
	/* done: */		see_done
);
