/* Internal inactivity timer. */
/* $Id: timer.c,v 1.14 2004/12/02 16:34:01 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "config/options.h"
#include "lowlevel/select.h"
#include "lowlevel/timer.h"
#include "sched/event.h"
#include "terminal/event.h"
#include "terminal/terminal.h"


/* Timer for periodically saving configuration files to disk */
static int periodic_save_timer = -1;

static int countdown = -1;

int timer_duration = 0;

static void
count_down(void *xxx)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, -1, 0, 0);
	struct keybinding *kb;
	struct terminal *terminal;

	timer_duration--;
	if (timer_duration) {
		countdown = install_timer(1000, count_down, NULL);
		return;
	} else {
		countdown = -1;
	}

	kb = kbd_nm_lookup(KEYMAP_MAIN, get_opt_str("ui.timer.action"), NULL);
	if (kb) {
		ev.info.keyboard.key = kb->key;
		ev.info.keyboard.modifier = kb->meta;

		foreach (terminal, terminals) {
			term_send_event(terminal, &ev);
		}
	}

	reset_timer();
}

void
reset_timer(void)
{
	if (countdown >= 0) {
		kill_timer(countdown);
		countdown = -1;
	}

	if (!get_opt_int("ui.timer.enable")) return;

	timer_duration = get_opt_int("ui.timer.duration");
	countdown = install_timer(1000, count_down, NULL);
}


static void
periodic_save_handler(void *xxx)
{
	static int periodic_save_event_id = EVENT_NONE;
	int interval;

	if (get_cmd_opt_bool("anonymous")) return;

	/* Don't trigger anything at startup */
	if (periodic_save_event_id == EVENT_NONE)
		set_event_id(periodic_save_event_id, "periodic-saving");
	else
		trigger_event(periodic_save_event_id);

	interval = get_opt_int("infofiles.save_interval") * 1000;
	if (!interval) return;

	periodic_save_timer = install_timer(interval, periodic_save_handler, NULL);
}

static int
periodic_save_change_hook(struct session *ses, struct option *current,
			  struct option *changed)
{
	if (get_cmd_opt_bool("anonymous")) return 0;

	if (periodic_save_timer != -1) {
		kill_timer(periodic_save_timer);
		periodic_save_timer = -1;
	}

	periodic_save_handler(NULL);

	return 0;
}


void
init_timer(void)
{
	struct change_hook_info timer_change_hooks[] = {
		{ "infofiles.save_interval", periodic_save_change_hook },
		{ NULL,	NULL },
	};

	register_change_hooks(timer_change_hooks);
	periodic_save_handler(NULL);
	reset_timer();
}

void
done_timer(void)
{
	if (periodic_save_timer >= 0) kill_timer(periodic_save_timer);
	if (countdown >= 0) kill_timer(countdown);
}
