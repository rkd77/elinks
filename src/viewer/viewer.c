/* Viewer module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "viewer/text/marks.h"
#include "viewer/text/search.h"
#include "viewer/timer.h"

static struct module *viewer_submodules[] = {
	&search_history_module,
	&timer_module,
#ifdef CONFIG_MARKS
	&viewer_marks_module,
#endif
	NULL
};

struct module viewer_module = struct_module(
	/* name: */		N_("Viewer"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	viewer_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
