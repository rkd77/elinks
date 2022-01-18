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

#ifdef CONFIG_BROTLI
#include "encoding/brotli.h"
#endif
#ifdef CONFIG_GZIP
#include "encoding/gzip.h"
#endif
#ifdef CONFIG_BZIP2
#include "encoding/bzip2.h"
#endif
#ifdef CONFIG_LZMA
#include "encoding/lzma.h"
#endif
#ifdef CONFIG_ZSTD
#include "encoding/zstd.h"
#endif

#include "intl/libintl.h"
#include "main/module.h"
#include "main/select.h"
#include "main/version.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "vernum.h"

#if defined(CONFIG_LIBEV) || defined(CONFIG_LIBEVENT)
extern int event_enabled;
#endif 

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
	int i;

	foreach_module (module, builtin_modules, i) {
		if (i > 0) add_to_string(string, ", ");
		add_module_to_string(string, module, term);
	}
}

/* Wrap string on spaces starting at position @start_at, trying
 * to keep lines undex @maxlen length */
static void
wrap_string(struct string *string, int start_at, int maxlen)
{
	char *pos, *start_pos;
	char *last_pos = NULL;

	assert(string && string->source && start_at < string->length);
	if_assert_failed return;

	if (maxlen <= 0) return;

	pos = start_pos = &string->source[start_at];
	while ((pos = strchr(pos, ' '))) {
		int len = pos - start_pos;

		if (len < maxlen) {
			last_pos = pos;
			pos++;
		} else {
			if (last_pos) *last_pos = '\n';
			pos = start_pos = last_pos + 1;
		}
		if (!*pos) break;
	}
}

/* @more will add more information especially for info box. */
char *
get_dyn_full_version(struct terminal *term, int more)
{
	static unsigned char comma[] = ", ";
	struct string string;

	if (!init_string(&string)) return NULL;

	add_format_to_string(&string, "ELinks %s", VERSION_STRING);
	if (*build_id) {
		add_char_to_string(&string, more ? '\n' : ' ');
		add_format_to_string(&string, "%s", build_id);
	}

	add_char_to_string(&string, '\n');
	add_format_to_string(&string, _("Built on %s %s", term),
			     build_date, build_time);

	if (more) {
		add_to_string(&string, "\n\n");
		add_to_string(&string, _("Text WWW browser", term));
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
#ifdef CONFIG_BROTLI
		comma, "brotli(", get_brotli_version(),")",
#endif
#ifdef CONFIG_GZIP
		comma, "gzip(", get_gzip_version(),")",
#endif
#ifdef CONFIG_BZIP2
		comma, "bzip2(", get_bzip2_version(), ")",
#endif
#ifdef CONFIG_LZMA
		comma, "lzma(", get_lzma_version(), ")",
#endif
#ifdef CONFIG_ZSTD
		comma, "zstd(", get_zstd_version(), ")",
#endif
#ifndef CONFIG_MOUSE
		comma, _("No mouse", term),
#endif
#ifdef CONFIG_UTF8
		comma, "UTF-8",
#endif
#ifdef CONFIG_COMBINE
		comma, _("Combining characters", term),
#endif
#ifdef CONFIG_LIBEV
		comma, (event_enabled ? _("libev", term) : _("libev (disabled)", term)), "(", get_libevent_version(), ")",
#endif
#ifdef CONFIG_LIBEVENT
		comma, (event_enabled ? _("libevent", term) : _("libevent (disabled)", term)), "(", get_libevent_version(), ")",
#endif
#ifdef CONFIG_TERMINFO
		comma, (get_cmd_opt_bool("terminfo") ? _("terminfo", term) : _("terminfo (disabled)", term)),
#endif
		comma,
		(char *) NULL
	);

	add_modules_to_string(&string, term);

	if (!more) {
		int start_at = 0;
		char *last_newline = strrchr((const char *)string.source, '\n');

		if (last_newline) {
			start_at = last_newline - string.source + 1;
		}

		wrap_string(&string, start_at, 72);
	}

	return string.source;
}

/* This one is used to prevent usage of straconcat() at backtrace time. */
void
init_static_version(void)
{
	char *s = get_dyn_full_version((struct terminal *) NULL, 0);

	if (s) {
		safe_strncpy(full_static_version, s, sizeof(full_static_version));
		mem_free(s);
	}
}
