#ifndef EL__OSDEP_NEWWIN_H
#define EL__OSDEP_NEWWIN_H

#include "terminal/terminal.h"

/* {struct open_in_new} and @open_in_new is used for setting up menues
 * of how new windows can be opened. */
struct open_in_new {
	enum term_env_type env;	/* The term->environment the entry covers */
	unsigned char *command;	/* The default command for openning a window */
	unsigned char *text;	/* The menu text */
};

/* The table containing all the possible environment types */
extern const struct open_in_new open_in_new[];

/* Iterator for the @open_in_new table over a terminal environment bitmap. */
#define foreach_open_in_new(i, term_env) \
	for ((i) = 0; open_in_new[(i)].env; (i)++) \
		if (((term_env) & open_in_new[(i)].env))

/* Returns the number of possible ways to open new windows using the
 * environment of @term. */
int can_open_in_new(struct terminal *term);

/* Opens a new window with the executable @exe_name under the given terminal
 * @environment and passing the arguments in the string @param.
 *
 * For the ENV_XWIN environment, @exe_name being 'elinks' and @param empty the
 * window will be opened using: 'xterm -e elinks' */
void open_new_window(struct terminal *term, unsigned char *exe_name,
		     enum term_env_type environment, unsigned char *param);

#endif
