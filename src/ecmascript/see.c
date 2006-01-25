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



/*** The ELinks interface */

void
ecmascript_init(struct module *module)
{
	see_init();
}

void
ecmascript_done(struct module *module)
{
	see_done();
}

struct ecmascript_interpreter *
ecmascript_get_interpreter(struct view_state *vs)
{
	struct ecmascript_interpreter *interpreter;

	assert(vs);

	interpreter = mem_calloc(1, sizeof(*interpreter));
	if (!interpreter)
		return NULL;

	interpreter->vs = vs;
	init_list(interpreter->onload_snippets);
	see_get_interpreter(interpreter);

	return interpreter;
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	see_put_interpreter(interpreter);
	free_string_list(&interpreter->onload_snippets);
	mem_free(interpreter);
}

void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code)
{
	if (!get_ecmascript_enable())
		return;
	assert(interpreter);
	see_eval(interpreter, code);
}

unsigned char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	if (!get_ecmascript_enable())
		return NULL;
	assert(interpreter);
	return see_eval_stringback(interpreter, code);
}

int
ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter,
			 struct string *code)
{
	if (!get_ecmascript_enable())
		return -1;
	assert(interpreter);
	return see_eval_boolback(interpreter, code);
}

void
see_init(void)
{
	init_intern_strings();
}

void
see_done(void)
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

void
see_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code)
{

	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct SEE_input *input = SEE_input_elinks(interp, code->source);
	SEE_try_context_t try_ctxt;
	struct SEE_value result;

	g->exec_start = time(NULL);
	SEE_TRY(interp, try_ctxt) {
		SEE_Global_eval(interp, input, &result);
	}

	SEE_INPUT_CLOSE(input);
	SEE_CAUGHT(try_ctxt);
}


unsigned char *
see_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	struct SEE_interpreter *interp = interpreter->backend_data;
	struct global_object *g = (struct global_object *)interp;
	struct SEE_input *input = SEE_input_elinks(interp, code->source);
	SEE_try_context_t try_ctxt;
	struct SEE_value result;
	/* 'volatile' qualifier prevents register allocation which fixes:
	 *  warning: variable 'xxx' might be clobbered by 'longjmp' or 'vfork'
	 */
	unsigned char *volatile string = NULL;

	g->exec_start = time(NULL);
	SEE_TRY(interp, try_ctxt) {
		SEE_Global_eval(interp, input, &result);
		if (SEE_VALUE_GET_TYPE(&result) != SEE_NULL)
			string = SEE_value_to_unsigned_char(interp, &result);

	}
	SEE_INPUT_CLOSE(input);
	if (SEE_CAUGHT(try_ctxt)) {
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
	struct SEE_input *input = SEE_input_elinks(interp, code->source);
	SEE_try_context_t try_ctxt;
	struct SEE_value result;
	/* 'volatile' qualifier prevents register allocation which fixes:
	 *  warning: variable 'xxx' might be clobbered by 'longjmp' or 'vfork'
	 */
	SEE_int32_t volatile res = 0;

	g->exec_start = time(NULL);
	SEE_TRY(interp, try_ctxt) {
		SEE_Global_eval(interp, input, &result);
		/* history.back() returns SEE_NULL */
		if (SEE_VALUE_GET_TYPE(&result) == SEE_NULL)
			res = 0;
		else
			res = SEE_ToInt32(interp, &result);
	}

	SEE_INPUT_CLOSE(input);
	if (SEE_CAUGHT(try_ctxt)) {
		return -1;
	}
	return res;
}

struct module see_module = struct_module(
	/* name: */		"SEE",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
