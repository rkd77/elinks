/* General scripting system functionality */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/scripting.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"


/* Backends dynamic area: */

#include "scripting/guile/guile.h"
#include "scripting/lua/lua.h"
#include "scripting/perl/perl.h"
#include "scripting/python/python.h"
#include "scripting/ruby/ruby.h"
#include "scripting/see/see.h"


/* Error reporting. */

#if defined(CONFIG_RUBY) || defined(CONFIG_SEE)
void
report_scripting_error(struct module *module, struct session *ses,
		       unsigned char *msg)
{
	struct terminal *term;
	struct string string;

	if (!ses) {
		if (list_empty(terminals)) {
			usrerror("[%s error] %s", module->name, msg);
			return;
		}

		term = terminals.next;

	} else {
		term = ses->tab->term;
	}

	if (!init_string(&string))
		return;

	add_format_to_string(&string,
		_("An error occurred while running a %s script", term),
		module->name);

	add_format_to_string(&string, ":\n\n%s", msg);

	info_box(term, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		 N_("Browser scripting error"), ALIGN_LEFT, string.source);
}
#endif


static struct module *scripting_modules[] = {
#ifdef CONFIG_LUA
	&lua_scripting_module,
#endif
#ifdef CONFIG_GUILE
	&guile_scripting_module,
#endif
#ifdef CONFIG_PERL
	&perl_scripting_module,
#endif
#ifdef CONFIG_PYTHON
	&python_scripting_module,
#endif
#ifdef CONFIG_RUBY
	&ruby_scripting_module,
#endif
#ifdef CONFIG_SEE
	&see_scripting_module,
#endif
	NULL,
};

struct module scripting_module = struct_module(
	/* name: */		N_("Scripting"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	scripting_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
