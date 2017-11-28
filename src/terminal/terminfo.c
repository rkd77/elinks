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

int
terminfo_setupterm(char *term, int fildes)
{
	return setupterm(term, fildes, NULL);
}

char *
terminfo_clear_screen(void)
{
	char *res = tiparm(clear_screen);

	if (res) return res;
	return "";
}

char *
terminfo_set_bold(int arg)
{
	char *res = tiparm(arg ? enter_bold_mode : exit_attribute_mode);

	if (res) return res;
	return "";
}

char *
terminfo_set_italics(int arg)
{
	char *res = tiparm(arg ? enter_italics_mode : exit_italics_mode);

	if (res) return res;
	return "";
}

char *
terminfo_set_underline(int arg)
{
	char *res = tiparm(arg ? enter_underline_mode : exit_underline_mode);

	if (res) return res;
	return "";
}

char *
terminfo_set_background(int arg)
{
	char *res = tiparm(set_a_background, arg);

	if (res) return res;
	return "";
}

char *
terminfo_set_foreground(int arg)
{
	char *res = tiparm(set_a_foreground, arg);

	if (res) return res;
	return "";
}

int
terminfo_max_colors(void)
{
	return max_colors;
}

char *
terminfo_cursor_address(int y, int x)
{
	char *res = tiparm(cursor_address, y, x);

	if (res) return res;
	return "";
}
