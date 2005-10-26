/* SEE interface (scripting engine) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/home.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/scripting.h"
#include "scripting/see/core.h"
#include "scripting/see/interface.h"
#include "scripting/see/see.h"
#include "util/error.h"
#include "util/string.h"


#define SEE_HOOKS_FILENAME	"hooks.js"

struct SEE_interpreter see_interpreter;
struct session *see_ses;


struct string *
convert_see_string(struct string *string, struct SEE_string *source)
{
	unsigned int pos;

	if (!init_string(string))
		return NULL;

	for (pos = 0; pos < source->length; pos++) {
		add_char_to_string(string, (unsigned char) source->data[pos]);
	}

	return string;
}


/* Error reporting. */

void
alert_see_error(struct session *ses, unsigned char *msg)
{
	report_scripting_error(&see_scripting_module, ses, msg);
}


static void
see_abort_handler(struct SEE_interpreter *see, const char *msg)
{
	ERROR((unsigned char *) msg);
	/* TODO: reload scripts! */
}

static void
see_oom_handler(struct SEE_interpreter *see)
{
	/* XXX: Ignore! */
}

void
init_see(struct module *module)
{
	struct SEE_interpreter *see = &see_interpreter;
	unsigned char *path;
	FILE *file;

	SEE_abort = see_abort_handler;
	SEE_mem_exhausted_hook = see_oom_handler;

	/* Initialise an interpreter */
	SEE_interpreter_init(see);

	/* Set up the ELinks interface. */
	init_see_interface(see);

	if (elinks_home) {
		path = straconcat(elinks_home, SEE_HOOKS_FILENAME, NULL);

	} else {
		path = stracpy(CONFDIR "/" SEE_HOOKS_FILENAME);
	}

	if (!path) return;

	file = fopen(path, "r");
	if (file) {
		struct SEE_value result;
		struct SEE_input *input;
		SEE_try_context_t try_context;
		struct SEE_value *exception;
		struct string error_msg;

		/* Load ~/.elinks/hooks.js into the interpreter. */
		input = SEE_input_file(see, file, path, NULL);
		SEE_TRY(see, try_context) {
			SEE_Global_eval(see, input, &result);
		}

		SEE_INPUT_CLOSE(input);

		exception = SEE_CAUGHT(try_context);
		if (exception && init_string(&error_msg)) {
			SEE_try_context_t try_context2;
			struct SEE_value value;

			SEE_TRY(see, try_context2) {
				SEE_ToString(see, exception, &value);
				convert_see_string(&error_msg, value.u.string);
#if 0
				if (ctxt.throw_file)
					fprintf(stderr, "  (thrown from %s:%d)\n", 
						ctxt.throw_file, ctxt.throw_line);
#endif
			}

			WDBG("errors encountered while reading %s:\n%s", path, error_msg.source);
			done_string(&error_msg);

			if (SEE_CAUGHT(try_context2)) {
				WDBG("exception thrown while "
					"printing exception");
#if 0
				if (ctxt2.throw_file)
					fprintf(stderr, " at %s:%d",
						ctxt2.throw_file, ctxt2.throw_line);
#endif
			}
		}
	}

	mem_free(path);
}
