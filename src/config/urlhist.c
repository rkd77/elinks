/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.35 2004/11/19 16:42:35 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/urlhist.h"
#include "sched/event.h"
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

struct event_hook_info goto_url_history_hooks[] = {
	{ "periodic-saving", goto_url_history_write_hook, NULL },

	NULL_EVENT_HOOK_INFO,
};

void
init_url_history(void)
{
	load_url_history();
	register_event_hooks(goto_url_history_hooks);
}

void
done_url_history(void)
{
	unregister_event_hooks(goto_url_history_hooks);
	save_url_history();
	free_list(goto_url_history.entries);
}
