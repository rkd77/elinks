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
terminfo_set_bold(int arg)
{
	return tiparm(arg ? enter_bold_mode : exit_attribute_mode);
}

char *
terminfo_set_italics(int arg)
{
	return tiparm(arg ? enter_italics_mode : exit_italics_mode);
}

char *
terminfo_set_underline(int arg)
{
	return tiparm(arg ? enter_underline_mode : exit_underline_mode);
}

char *
terminfo_set_background(int arg)
{
	return tiparm(set_a_background, arg);
}

char *
terminfo_set_foreground(int arg)
{
	return tiparm(set_a_foreground, arg);
}
