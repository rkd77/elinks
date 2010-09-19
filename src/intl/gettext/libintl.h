
#ifndef EL__INTL_GETTEXT_LIBINTL_H
#define EL__INTL_GETTEXT_LIBINTL_H

/* This header file provides an interface between ELinks and GNU libintl. I was
 * astonished that there is no libintl.h but libgnuintl.h (and that name seemed
 * ugly ;-), and I also decided that it will be better to introduce a clean
 * interface instead of digging to libgnuintl.h too much. */

/* Contrary to a standard gettext interface, we pass destination charset and
 * language (in form of struct terminal) directly with each call, allowing to
 * easily multiplex between several terminals. */

#include "intl/gettext/libgettext.h"

#include "config/options.h"
#include "intl/charsets.h"
#include "terminal/terminal.h"

/* no-op - just for marking */
#define N_(msg) (gettext_noop(msg))

#ifndef CONFIG_SMALL
#define N__(msg) (gettext_noop(msg))
#else
#define N__(msg) (NULL)
#endif


/* The intl/gettext/libgettext.h header nukes gettext functions but not the _()
 * function so make sure it is also just a noop when NLS is disabled. */
#ifndef CONFIG_NLS

/* In order to make it able to compile using -Werror this has to be a function
 * so that local @term variables will not be reported as unused. */
static inline unsigned char *
_(unsigned char *msg, struct terminal *term)
{
	return gettext_noop(msg);
}

static inline unsigned char *
n_(unsigned char *msg1, unsigned char *msg2, unsigned long int n, struct terminal *term)
{
	return gettext_noop(msg1);
}

static inline void
intl_set_charset_by_index(int new_charset)
{
}

#else

/* The number of the charset to which the "elinks" domain was last
 * bound with bind_textdomain_codeset(), or -1 if none yet.  This
 * cannot be a static variable in _(), because then it would get
 * duplicated in every translation unit, even though the actual
 * binding is global.  */
extern int current_charset;

/* Define it to find redundant useless calls */
/* #define DEBUG_IT */

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

/* TODO: Ideally, we should internally work only in Unicode - then the need for
 * charsets multiplexing would cease. That'll take some work yet, though.
 * --pasky */

#ifndef DEBUG_IT

/* Wraps around gettext(), employing charset multiplexing. If you don't care
 * about charset (usually during initialization or when you don't use terminals
 * at all), use gettext() directly. */
static inline unsigned char *
_(unsigned char *msg, struct terminal *term)
{
	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg || !*msg)
		return msg;

	if (term) intl_set_charset(term);

	return (unsigned char *) gettext(msg);
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
static inline unsigned char *
__(unsigned char *file, unsigned int line, unsigned char *func,
   unsigned char *msg, struct terminal *term)
{
	static unsigned char last_file[512] = "";
	static unsigned int last_line = 0;
	static unsigned char last_func[1024] = "";
	static unsigned char last_result[16384] = "";
	unsigned char *result;

	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg || !*msg) {
		ERROR("%s:%u %s msg parameter", file, line, msg ? "empty": "NULL");
		return msg;
	}

	if (term) intl_set_charset(term);

	result = (unsigned char *) gettext(msg);

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
static inline unsigned char *
n_(unsigned char *msg1, unsigned char *msg2, unsigned long int n, struct terminal *term)
{
	/* Prevent useless (and possibly dangerous) calls. */
	if (!msg1 || !*msg1)
		return msg1;

	if (term) intl_set_charset(term);

	return (unsigned char *) ngettext(msg1, msg2, n);
}


/* Languages table lookups. */

struct language {
	unsigned char *name;
	unsigned char *iso639;
};

extern struct language languages[];

/* These two calls return 1 (english) if the code/name wasn't found. */
extern int name_to_language(const unsigned char *name);
extern int iso639_to_language(unsigned char *iso639);

extern unsigned char *language_to_name(int language);
extern unsigned char *language_to_iso639(int language);

extern int get_system_language_index(void);

/* The current state. The state should be initialized by a set_language(0)
 * call. */

extern int current_language, system_language;
extern void set_language(int language);

#endif /* CONFIG_NLS */

#endif
