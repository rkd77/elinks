/** Terminfo interfaces
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_TERM_H
#include <term.h>
#endif

#include "elinks.h"
#include "terminfo.h"

int
terminfo_setupterm(char *term, int fildes)
{
	ELOG
	return setupterm(term, fildes, NULL);
}

const char *
terminfo_clear_screen(void)
{
	ELOG
	char *res = tiparm(clear_screen);

	return res ?: "";
}

const char *
terminfo_set_bold(int arg)
{
	ELOG
	char *res = tiparm(arg ? enter_bold_mode : exit_attribute_mode);

	return res ?: "";
}

const char *
terminfo_set_italics(int arg)
{
	ELOG
	char *res = tiparm(arg ? enter_italics_mode : exit_italics_mode);

	return res ?: "";
}

const char *
terminfo_set_underline(int arg)
{
	ELOG
	char *res = tiparm(arg ? enter_underline_mode : exit_underline_mode);

	return res ?: "";
}

const char *
terminfo_set_strike(int arg)
{
	ELOG
	char *res = tigetstr(arg ? "smxx" : "rmxx");

	return res ?: "";
}

const char *
terminfo_set_background(int arg)
{
	ELOG
	char *res = tiparm(set_a_background, arg);

	return res ?: "";
}

const char *
terminfo_set_foreground(int arg)
{
	ELOG
	char *res = tiparm(set_a_foreground, arg);

	return res ?: "";
}

const char *
terminfo_set_standout(int arg)
{
	ELOG
	char *res = tiparm(arg ? enter_standout_mode : exit_standout_mode);

	return res ?: "";
}

int
terminfo_max_colors(void)
{
	ELOG
	return max_colors;
}

const char *
terminfo_cursor_address(int y, int x)
{
	ELOG
	char *res = tiparm(cursor_address, y, x);

	return res ?: "";
}
