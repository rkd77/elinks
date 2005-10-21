/* Manipulation with file containing URL history */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/urlhist.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/module.h"
#include "util/lists.h"
#include "util/memory.h"

#define GOTO_HISTORY_FILENAME		"gotohist"


INIT_INPUT_HISTORY(goto_url_history);

static void
load_url_history(void)
{
	load_input_history(&goto_url_history, GOTO_HISTORY_FILENAME);
}

static void
save_url_history(void)
{
	save_input_history(&goto_url_history, GOTO_HISTORY_FILENAME);
}

static enum evhook_status
goto_url_history_write_hook(va_list ap, void *data)
{
	save_url_history();
	return EVENT_HOOK_STATUS_NEXT;
}

static struct event_hook_info goto_url_history_hooks[] = {
	{ "periodic-saving", 0, goto_url_history_write_hook, NULL },

	NULL_EVENT_HOOK_INFO,
};

static void
init_url_history(struct module *module)
{
	load_url_history();
}

static void
done_url_history(struct module *module)
{
	save_url_history();
	free_list(goto_url_history.entries);
}

struct module goto_url_history_module = struct_module(
	/* name: */		N_("Goto URL History"),
	/* options: */		NULL,
	/* hooks: */		goto_url_history_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_url_history,
	/* done: */		done_url_history
);
