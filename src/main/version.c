/* Version information */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/version.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "vernum.h"


static void
add_module_to_string(struct string *string, struct module *module,
		     struct terminal *term)
{
	struct module *submodule;
	int i;

	if (module->name) add_to_string(string, _(module->name, term));

	if (!module->submodules) return;

	add_to_string(string, " (");

	foreach_module (submodule, module->submodules, i) {
		if (i > 0) add_to_string(string, ", ");
		add_module_to_string(string, submodule, term);
	}

	add_to_string(string, ")");
}

static void
add_modules_to_string(struct string *string, struct terminal *term)
{
	struct module *module;
	int i, last_split = 0;
	unsigned char *last_newline = strrchr(string->source, '\n');

	if (last_newline)
		last_split = last_newline - string->source;

	foreach_module_builtin (module, builtin_modules, i) {
		if (i > 0) add_to_string(string, ", ");
		if (string->length - last_split > 70) {
			add_char_to_string(string, '\n');
			last_split = string->length;
		}
		add_module_to_string(string, module, term);
	}
}

/* @more will add more information especially for info box. */
unsigned char *
get_dyn_full_version(struct terminal *term, int more)
{
	static const unsigned char comma[] = ", ";
	struct string string;

	if (!init_string(&string)) return NULL;

	add_format_to_string(&string, "ELinks %s", VERSION_STRING);
	if (*build_id)
		add_format_to_string(&string, " (%s)", build_id);
	if (more) {
		add_to_string(&string, "\n");
		add_format_to_string(&string, _("Built on %s %s", term),
					build_date, build_time);
		add_to_string(&string, "\n\n");
		add_to_string(&string, _("Text WWW browser", term));
	} else {
		add_format_to_string(&string, _(" (built on %s %s)", term),
					build_date, build_time);
	}

	string_concat(&string,
		"\n\n",
		_("Features:", term), "\n",
#ifndef CONFIG_DEBUG
		_("Standard", term),
#else
		_("Debug", term),
#endif
#ifdef CONFIG_FASTMEM
		comma, _("Fastmem", term),
#endif
#ifdef CONFIG_OWN_LIBC
		comma, _("Own Libc Routines", term),
#endif
#ifndef CONFIG_BACKTRACE
		comma, _("No Backtrace", term),
#endif
#ifdef CONFIG_IPV6
		comma, "IPv6",
#endif
#ifdef CONFIG_GZIP
		comma, "gzip",
#endif
#ifdef CONFIG_BZIP2
		comma, "bzip2",
#endif
#ifdef CONFIG_LZMA
		comma, "lzma",
#endif
#ifndef CONFIG_MOUSE
		comma, _("No mouse", term),
#endif
#ifdef CONFIG_UTF8
		comma, "UTF-8",
#endif
		comma,
		NULL
	);

	add_modules_to_string(&string, term);

	return string.source;
}

/* This one is used to prevent usage of straconcat() at backtrace time. */
void
init_static_version(void)
{
	unsigned char *s = get_dyn_full_version((struct terminal *) NULL, 0);

	if (s) {
		safe_strncpy(full_static_version, s, sizeof(full_static_version));
		mem_free(s);
	}
}
