/* Get system name */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include "elinks.h"

#include "osdep/sysname.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char system_name[MAX_STR_LEN];

#ifdef HAVE_POPEN
static int
got_it_from_uname_command(void)
{
	FILE *f;
	unsigned char *p;

	f = popen("uname -srm", "r");
	if (!f) return 0;

	if (fread(system_name, 1, sizeof(system_name) - 1, f) <= 0) {
		pclose(f);
		return 0;
	}

	pclose(f);

	system_name[MAX_STR_LEN - 1] = '\0'; /* Safer. */
	p = system_name;
	while (*p >= ' ') p++;
	*p = '\0';

	if (system_name[0])
		return 1;

	return 0;
}
#else
#define got_it_from_uname_command() 0
#endif

void
get_system_name(void)
{
#if defined(HAVE_SYS_UTSNAME_H) && defined(HAVE_UNAME)
	struct utsname name;

	if (!uname(&name)) {
		snprintf(system_name, sizeof(system_name),
			 "%s %s %s", name.sysname, name.release, name.machine);
		return;
	}
#endif

	if (got_it_from_uname_command()) return;

	safe_strncpy(system_name, SYSTEM_NAME, sizeof(system_name));
}
