/* Shared protocol functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* FreeBSD needs this before resource.h */
#endif
#include <sys/types.h> /* FreeBSD needs this before resource.h */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "protocol/common.h"


/* Close all non-terminal file descriptors. */
void
close_all_non_term_fd(void)
{
	int n;
	int max = 1024;
#ifdef RLIMIT_NOFILE
	struct rlimit lim;

	if (!getrlimit(RLIMIT_NOFILE, &lim))
		max = lim.rlim_max;
#endif
	for (n = 3; n < max; n++)
		close(n);
}
