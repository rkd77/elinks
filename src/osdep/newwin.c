/* Open in new window handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "osdep/newwin.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef XTERM
#define XTERM_CMD XTERM
#else
#define XTERM_CMD DEFAULT_XTERM_CMD
#endif

const struct open_in_new open_in_new[] = {
	{ ENV_XWIN,	XTERM_CMD,		    N_("~Xterm") },
	{ ENV_TWIN,	DEFAULT_TWTERM_CMD,	    N_("T~wterm") },
	{ ENV_SCREEN,	DEFAULT_SCREEN_CMD,	    N_("~Screen") },
#ifdef CONFIG_OS_OS2
	{ ENV_OS2VIO,	DEFAULT_OS2_WINDOW_CMD,	    N_("~Window") },
	{ ENV_OS2VIO,	DEFAULT_OS2_FULLSCREEN_CMD, N_("~Full screen") },
#endif
#ifdef CONFIG_OS_WIN32
	{ ENV_WIN32,	"",			    N_("~Window") },
#endif
#ifdef CONFIG_OS_BEOS
	{ ENV_BE,	DEFAULT_BEOS_TERM_CMD,	    N_("~BeOS terminal") },
#endif
	{ 0, NULL, NULL }
};

int
can_open_in_new(struct terminal *term)
{
	int i, possibilities = 0;

	foreach_open_in_new (i, term->environment) {
		possibilities++;
	}

	return possibilities;
}

void
open_new_window(struct terminal *term, unsigned char *exe_name,
		enum term_env_type environment, unsigned char *param)
{
	unsigned char *command = NULL;
	int i;

	foreach_open_in_new (i, environment) {
		command = open_in_new[i].command;
		break;
	}

	assert(command);

	if (environment & ENV_XWIN) {
		unsigned char *xterm = getenv("ELINKS_XTERM");

		if (!xterm) xterm = getenv("LINKS_XTERM");
		if (xterm) command = xterm;

	} else if (environment & ENV_TWIN) {
		unsigned char *twterm = getenv("ELINKS_TWTERM");

		if (!twterm) twterm = getenv("LINKS_TWTERM");
		if (twterm) command = twterm;
	}

	command = straconcat(command, " ", exe_name, " ", param, NULL);
	if (!command) return;

	exec_on_terminal(term, command, "", 2);
	mem_free(command);
}
