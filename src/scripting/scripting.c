/* General scripting system functionality */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/msgbox.h"
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
#include "scripting/smjs/smjs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


/* Error reporting. */

#if defined(CONFIG_SCRIPTING_RUBY) || defined(CONFIG_SCRIPTING_SPIDERMONKEY) || defined(CONFIG_SCRIPTING_PYTHON)
void
report_scripting_error(struct module *module, struct session *ses,
		       unsigned char *msg)
{
	struct terminal *term;
	struct string string;

	if (!ses) {
		term = get_default_terminal();
		if (term == NULL) {
			usrerror(gettext("[%s error] %s"),
				 gettext(module->name), msg);
			sleep(3);
			return;
		}

	} else {
		term = ses->tab->term;
	}

	if (!init_string(&string))
		return;

	add_format_to_string(&string,
		_("An error occurred while running a %s script", term),
		_(module->name, term));

	add_format_to_string(&string, ":\n\n%s", msg);

	info_box(term, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		 N_("Browser scripting error"), ALIGN_LEFT, string.source);
}
#endif


static struct module *scripting_modules[] = {
#ifdef CONFIG_SCRIPTING_LUA
	&lua_scripting_module,
#endif
#ifdef CONFIG_SCRIPTING_GUILE
	&guile_scripting_module,
#endif
#ifdef CONFIG_SCRIPTING_PERL
	&perl_scripting_module,
#endif
#ifdef CONFIG_SCRIPTING_PYTHON
	&python_scripting_module,
#endif
#ifdef CONFIG_SCRIPTING_RUBY
	&ruby_scripting_module,
#endif
#ifdef CONFIG_SCRIPTING_SPIDERMONKEY
	&smjs_scripting_module,
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
