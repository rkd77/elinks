/* Perl scripting engine */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "main/module.h"
#include "osdep/osdep.h"
#include "scripting/perl/core.h"
#include "util/file.h"

#define PERL_HOOKS_FILENAME	"hooks.pl"


PerlInterpreter *my_perl;

#ifdef PERL_SYS_INIT3
extern char **environ;
#endif


static char *
get_global_hook_file(void)
{
	static char buf[] = CONFDIR STRING_DIR_SEP PERL_HOOKS_FILENAME;

	if (file_exists(buf)) return buf;
	return NULL;
}

static char *
get_local_hook_file(void)
{
	static char buf[1024];	/* TODO: MAX_PATH ??? --Zas */

	if (!elinks_home) return NULL;
	snprintf(buf, sizeof(buf), "%s%s", elinks_home, PERL_HOOKS_FILENAME);
	if (file_exists(buf)) return buf;
	return NULL;
}


static void
precleanup_perl(struct module *module)
{
	if (!my_perl) return;

	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
}

void
cleanup_perl(struct module *module)
{
	precleanup_perl(module);
#ifdef PERL_SYS_TERM
	PERL_SYS_TERM();
#endif
}


void
init_perl(struct module *module)
{
	/* FIXME: it seems that some systems like OS/2 requires PERL_SYS_INIT3
	 * and PERL_SYS_TERM to open/close the same block, at least regarding
	 * some ml messages.
	 *
	 * Is passing @environ strictly needed ? --Zas */

	/* PERL_SYS_INIT3 may not be defined, it depends on the system. */
#ifdef PERL_SYS_INIT3
	char *my_argv[] = { NULL };
	int my_argc = 0;

	/* A hack to prevent unused variables warnings. */
	my_argv[my_argc++] = "";

	PERL_SYS_INIT3(&my_argc, &my_argv, &environ);
#endif

	my_perl = perl_alloc();
	if (my_perl) {
		char *hook_global = get_global_hook_file();
		char *hook_local = get_local_hook_file();
		char *global_argv[] = { "", hook_global};
		char *local_argv[] = { "", hook_local};
		int err = 1;

		perl_construct(my_perl);
		if (hook_local)
			err = perl_parse(my_perl, NULL, 2, local_argv, NULL);
		else if (hook_global)
			err = perl_parse(my_perl, NULL, 2, global_argv, NULL);
#ifdef PERL_EXIT_DESTRUCT_END
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#endif
		if (!err) err = perl_run(my_perl);
		if (err) precleanup_perl(module);
	}
}
