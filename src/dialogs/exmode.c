/* Ex-mode-like commandline support */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/conf.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/exmode.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/action.h"


/* The Ex-mode commandline is that blue-yellow thing which appears at the
 * bottom of the screen when you press ':' and lets you enter various commands
 * (just like in vi), especially actions, events (where they make sense) and
 * config-file commands. */


#define EXMODE_HISTORY_FILENAME		"exmodehist"

static INIT_INPUT_HISTORY(exmode_history);

typedef int (*exmode_handler_T)(struct session *, unsigned char *, unsigned char *);

static int
exmode_action_handler(struct session *ses, unsigned char *command,
		      unsigned char *args)
{
	enum main_action action_id = get_action_from_string(KEYMAP_MAIN, command);

	if (action_id == ACT_MAIN_NONE) return 0;
	if (action_id == ACT_MAIN_QUIT) action_id = ACT_MAIN_REALLY_QUIT;

	if (!*args)
		return do_action(ses, action_id, 0) != FRAME_EVENT_IGNORED;

	switch (action_id) {
		case ACT_MAIN_GOTO_URL:
			goto_url_with_hook(ses, args);
			return 1;
		default:
			break;
	}
	return 0;
}

static int
exmode_confcmd_handler(struct session *ses, unsigned char *command,
			unsigned char *args)
{
	enum parse_error err;

	assert(ses && command && args);

	if (get_cmd_opt_bool("anonymous"))
		return 0;

	/* Undo the arguments separation. */
	if (*args) *(--args) = ' ';

	err = parse_config_exmode_command(command);
	return err;
}

static const exmode_handler_T exmode_handlers[] = {
	exmode_action_handler,
	exmode_confcmd_handler,
	NULL,
};

static void
exmode_exec(struct session *ses, unsigned char buffer[INPUT_LINE_BUFFER_SIZE])
{
	/* First look it up as action, then try it as an event (but the event
	 * part should be thought out somehow yet, I s'pose... let's leave it
	 * off for now). Then try to evaluate it as configfile command. Then at
	 * least pop up an error. */
	unsigned char *command = buffer;
	unsigned char *args = command;
	int i;

	while (*command == ':') command++;

	if (!*command) return;

	add_to_input_history(&exmode_history, command, 1);

	skip_nonspace(args);
	if (*args) *args++ = 0;

	for (i = 0; exmode_handlers[i]; i++) {
		if (exmode_handlers[i](ses, command, args))
			break;
	}
}


static enum input_line_code
exmode_input_handler(struct input_line *input_line, int action_id)
{
	switch (action_id) {
		case ACT_EDIT_ENTER:
			exmode_exec(input_line->ses, input_line->buffer);
			return INPUT_LINE_CANCEL;

		default:
			return INPUT_LINE_PROCEED;
	}
}

void
exmode_start(struct session *ses)
{
	input_field_line(ses, ":", NULL, &exmode_history, exmode_input_handler);
}


static void
init_exmode(struct module *module)
{
	load_input_history(&exmode_history, EXMODE_HISTORY_FILENAME);
}

static void
done_exmode(struct module *module)
{
	save_input_history(&exmode_history, EXMODE_HISTORY_FILENAME);
	free_list(exmode_history.entries);
}

struct module exmode_module = struct_module(
	/* name: */		N_("Exmode"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_exmode,
	/* done: */		done_exmode
);
