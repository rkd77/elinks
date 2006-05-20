/* General module system functionality */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "main/module.h"


/* Dynamic area: */

#include "bfu/dialog.h"
#include "bookmarks/bookmarks.h"
#include "config/kbdbind.h"
#include "config/timer.h"
#include "config/urlhist.h"
#include "cookies/cookies.h"
#include "dialogs/exmode.h"
#include "document/css/css.h"
#include "document/document.h"
#include "ecmascript/ecmascript.h"
#include "formhist/formhist.h"
#include "globhist/globhist.h"
#include "mime/mime.h"
#include "network/ssl/ssl.h"
#include "protocol/protocol.h"
#include "scripting/scripting.h"
#include "terminal/terminal.h"
#include "viewer/viewer.h"


struct module *main_modules[] = {
	&document_module,
	&kbdbind_module,
	&terminal_module,
	NULL /* XXX: Keep this */
};

/* This is also used for version string composing so keep NULL terminated */
struct module *builtin_modules[] = {
	&periodic_saving_module,
	&viewer_module,
#ifdef CONFIG_CSS
	&css_module,
#endif
	&protocol_module,
#ifdef CONFIG_SSL
	&ssl_module,
#endif
	&mime_module,
#ifdef CONFIG_LEDS
	&leds_module,
#endif
#ifdef CONFIG_BOOKMARKS
	&bookmarks_module,
#endif
#ifdef CONFIG_COOKIES
	&cookies_module,
#endif
#ifdef CONFIG_ECMASCRIPT
	&ecmascript_module,
#endif
#ifdef CONFIG_FORMHIST
	&forms_history_module,
#endif
#ifdef CONFIG_GLOBHIST
	&global_history_module,
#endif
#ifdef CONFIG_SCRIPTING
	&scripting_module,
#endif
#ifdef CONFIG_EXMODE
	&exmode_module,
#endif
	&goto_url_history_module,
	NULL
};

/* Interface for handling single modules. */

void
register_module_options(struct module *module)
{
	struct module *submodule;
	int i;

	if (module->options) register_options(module->options, config_options);

	foreach_module (submodule, module->submodules, i) {
		register_module_options(submodule);
	}
}

void
unregister_module_options(struct module *module)
{
	struct module *submodule;
	int i;

	/* First knock out the submodules if any. */
	foreachback_module (submodule, module->submodules, i)
		unregister_module_options(submodule);

	if (module->options) unregister_options(module->options, config_options);
}

void
init_module(struct module *module)
{
	struct module *submodule;
	int i;

	if (module->init) module->init(module);
	if (module->hooks) register_event_hooks(module->hooks);

	foreach_module (submodule, module->submodules, i) {
		init_module(submodule);
	}
}

void
done_module(struct module *module)
{
	struct module *submodule;
	int i;

	/* First knock out the submodules if any. */
	foreachback_module (submodule, module->submodules, i)
		done_module(submodule);

	if (module->hooks) unregister_event_hooks(module->hooks);
	if (module->done) module->done(module);
}

/* Interface for handling builtin modules. */

void
register_modules_options(struct module *modules[])
{
	struct module *module;
	int i;

	foreach_module (module, modules, i) {
		register_module_options(module);
	}
}

void
unregister_modules_options(struct module *modules[])
{
	struct module *module;
	int i;

	/* Cleanups backward to initialization. */
	foreachback_module (module, modules, i)
		unregister_module_options(module);
}

/* TODO: We probably need to have two builtin module tables one for critical
 * modules that should always be used and one for optional (the ones controlled
 * by init_b in main.c */

void
init_modules(struct module *modules[])
{
	struct module *module;
	int i;

	foreach_module (module, modules, i) {
		init_module(module);
	}
}

void
done_modules(struct module *modules[])
{
	struct module *module;
	int i;

	/* Cleanups backward to initialization. */
	foreachback_module (module, modules, i)
		done_module(module);
}
