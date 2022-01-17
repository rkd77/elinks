#ifndef EL__INTL_LIBINTL_H
#define EL__INTL_LIBINTL_H

#include "config/options.h"
#include "intl/charsets.h"
#include "main/module.h"
#include "terminal/terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module gettext_module;

#ifdef CONFIG_GETTEXT
extern int _nl_msg_cat_cntr;

#include <libintl.h>

extern int current_charset;

/* no-op - just for marking */
#define N_(msg) (char *)(gettext_noop(msg))
#define gettext_noop(Str) (Str)

#ifndef CONFIG_SMALL
#define N__(msg) (gettext_noop(msg))
#else
#define N__(msg) (NULL)
#endif

static inline void
intl_set_charset_by_index(int new_charset)
{
	/* Prevent useless switching. */
	if (current_charset != new_charset) {
		bind_textdomain_codeset( /* PACKAGE */ "elinks",
					get_cp_mime_name(new_charset));
		current_charset = new_charset;
	}
}

static inline void
intl_set_charset(struct terminal *term)
{
	int new_charset = get_terminal_codepage(term);

	intl_set_charset_by_index(new_charset);
}

#ifndef DEBUG_IT

/* Wraps around gettext(), employing charset multiplexing. If you don't care
 * about charset (usually during initialization or when you don't use terminals
 * at all), use gettext() directly. */
static inline char *
_(const char *msg, struct terminal *term)
{
	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg || !*msg)
		return (char *)msg;

	if (term) intl_set_charset(term);

	return (char *) gettext(msg);
}

#else

#include "util/error.h"

/* This one will emit errors on null/empty msgs and when multiple calls are
 * done for the same result in the same function. Some noise is possible,
 * when a function is called twice or more, but then we should cache msg,
 * in function caller. --Zas */

/* __FUNCTION__ isn't supported by all, but it's debugging code. */
#define _(m, t) __(__FILE__, __LINE__, __FUNCTION__, m, t)

/* Overflows are theorically possible here. Debug purpose only. */
static inline char *
__(char *file, unsigned int line, char *func,
   char *msg, struct terminal *term)
{
	static char last_file[512] = "";
	static unsigned int last_line = 0;
	static char last_func[1024] = "";
	static char last_result[16384] = "";
	char *result;

	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg || !*msg) {
		ERROR("%s:%u %s msg parameter", file, line, msg ? "empty": "NULL");
		return msg;
	}

	if (term) intl_set_charset(term);

	result = (char *) gettext(msg);

	if (!strcmp(result, last_result)
	    && !strcmp(file, last_file)
	    && !strcmp(func, last_func)) {
		ERROR("%s:%u Duplicate call to _() in %s() (previous at line %u)",
		      file, line, func, last_line);
	}

	/* Risky ;) */
	strcpy(last_file, file);
	strcpy(last_func, func);
	strcpy(last_result, result);
	last_line = line;

	return result;
}

#endif

/* For plural handling. */
/* Wraps around ngettext(), employing charset multiplexing. If you don't care
 * about charset (usually during initialization or when you don't use terminals
 * at all), use ngettext() directly. */
static inline char *
n_(char *msg1, char *msg2, unsigned long int n, struct terminal *term)
{
	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg1 || !*msg1)
		return msg1;

	if (term) intl_set_charset(term);

	return (char *) ngettext(msg1, msg2, n);
}

extern char *EL_LANGUAGE;

/* Languages table lookups. */

struct language {
	char *name;
	char *iso639;
};

extern struct language languages[];

/* These two calls return 1 (english) if the code/name wasn't found. */
extern int name_to_language(const char *name);
extern int iso639_to_language(char *iso639);

extern char *language_to_name(int language);
extern char *language_to_iso639(int language);

extern int get_system_language_index(void);

/* The current state. The state should be initialized by a set_language(0)
 * call. */

extern int current_language, system_language;
extern void set_language(int language);

#else
#include "intl/gettext/libintl.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
