/* ECMAScript helper functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dialogs/status.h"
#include "ecmascript/ecmascript-c.h"
#include "intl/libintl.h"
#include "session/session.h"
#include "util/memory.h"
#include "util/string.h"

extern int interpreter_count;
extern int ecmascript_enabled;

int
ecmascript_get_interpreter_count(void)
{
	return interpreter_count;
}

void
toggle_ecmascript(struct session *ses)
{
	ecmascript_enabled = !ecmascript_enabled;

	if (ecmascript_enabled) {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript enabled", ses->tab->term)));
	} else {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript disabled", ses->tab->term)));
	}
	print_screen_status(ses);
}
