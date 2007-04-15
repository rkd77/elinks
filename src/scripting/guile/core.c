/* Guile interface (scripting engine) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libguile.h>

#include "elinks.h"

#include "config/home.h"
#include "main/module.h"
#include "scripting/guile/core.h"
#include "scripting/guile/hooks.h"
#include "util/error.h"
#include "util/file.h"
#include "util/string.h"

#define GUILE_HOOKS_FILENAME "hooks.scm"
#define GUILE_USERHOOKS_FILENAME "user-hooks.scm"

/*
 * Bindings
 */

/* static SCM c_current_url(void) */
/* { */
/* 	struct view_state *vs; */

/* 	if (have_location(ses) && (vs = ses ? &cur_loc(ses)->vs : 0)) */
/* 		return scm_makfrom0str(struri(vs->uri)); */
/* 	else */
/* 		return SCM_BOOL_F; */
/* } */
/* c_current_link */
/* c_current_title */
/* c_current_document */
/* c_current_document_formatted */
/* c_bind_key */
/* c_xdialog */


void
init_guile(struct module *module)
{
	SCM user_module;
	SCM internal_module;
	unsigned char *path;

	scm_init_guile();

	if (!elinks_home) return;

	/* Remember the current module. */
	user_module = scm_current_module();

	path = straconcat(elinks_home, GUILE_HOOKS_FILENAME,
			  (unsigned char *) NULL);
	if (!path) return;

	if (file_can_read(path)) {
		/* Load ~/.elinks/internal-hooks.scm. */
		scm_c_primitive_load_path(path);

		/* internal-hooks.scm should have created a new module (elinks
		 * internal).  Let's remember it, even though I haven't figured
		 * out how to use it directly yet... */
		internal_module = scm_current_module();

		/* Return to the user module, import bindings from (elinks
		 * internal), then load ~/.elinks/user-hooks.scm. */
		scm_set_current_module(user_module);

		/* FIXME: better way? i want to use internal_module directly */
		scm_c_use_module("elinks internal");
	}

	mem_free(path);

	path = straconcat(elinks_home, GUILE_USERHOOKS_FILENAME,
			  (unsigned char *) NULL);
	if (!path) return;
	if (file_can_read(path))
		scm_c_primitive_load_path(path);
	mem_free(path);
}
