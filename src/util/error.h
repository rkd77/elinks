/** Error handling and debugging stuff
 * @file
 *
 * Here you will found a chunk of functions useful for error states --- from
 * reporting of various problems to generic error tests/workarounds to some
 * tools to be used when you got into an error state already. Some of the
 * functions are also useful for debugging. */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H

/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
extern int errline;
extern const unsigned char *errfile;

/** @c DBG(format_string) is used for printing of debugging information. It
 * should not be used anywhere in the official codebase (although it is often
 * lying there commented out, as it may get handy). */
#undef DBG
#define DBG errfile = __FILE__, errline = __LINE__, elinks_debug
void elinks_debug(unsigned char *fmt, ...);

/** @c WDBG(format_string) is used for printing of debugging information, akin
 * to DBG(). However, it sleep(1)s, therefore being useful when it is going
 * to be overdrawn or so. It should not be used anywhere in the official
 * codebase (although it is often lying there commented out, as it may get
 * handy). */
#undef WDBG
#define WDBG errfile = __FILE__, errline = __LINE__, elinks_wdebug
void elinks_wdebug(unsigned char *fmt, ...);

/** @c ERROR(format_string) is used to report non-fatal unexpected errors during
 * the ELinks run. It tries to (not that agressively) draw user's attention to
 * the error, but never dumps core or so. Note that this should be used only in
 * cases of non-severe internal inconsistences etc, never as an indication of
 * user error (bad parameter, config file error etc.). We have usrerror() for
 * this kind of stuff, and there's nothing naughty about using that. */
#undef ERROR
#define ERROR errfile = __FILE__, errline = __LINE__, elinks_error
void elinks_error(unsigned char *fmt, ...);

/** @c INTERNAL(format_string) is used to report fatal errors during the ELinks
 * run. It tries to draw user's attention to the error and dumps core if ELinks
 * is running in the CONFIG_DEBUG mode. */
#undef INTERNAL
#define INTERNAL errfile = __FILE__, errline = __LINE__, elinks_internal
void elinks_internal(unsigned char *fmt, ...);


/** @c usrerror(format_string) is used to report user errors during a peaceful
 * ELinks run. It does not belong to the family above - it doesn't print code
 * location, beep nor sleep, it just wraps around fprintf(stderr, "...\n");. */
void usrerror(unsigned char *fmt, ...);


#ifdef HAVE_VARIADIC_MACROS
#ifdef CONFIG_DEBUG
/** The @c LOG_*() macros can be used to log to a file, however, by default log
 * messages are written to stderr. Set the following environment variables
 * to configure the log behavior:
 *
 * <dl>
 * <dt>ELINKS_LOG	<dd>The path to the log file, it is opened for appending
 * <dt>ELINKS_MSG	<dd>A comma separated list containing "error", "warn",
 *			    "info" and/or "debug" which can be used to limit
 *			    what messages to emit to the log.
 * <dt>ELINKS_FILES	<dd>A comma separated list of which files names to
 *			    emit log messages from.
 * </dl>
 */
void
elinks_log(unsigned char *msg, unsigned char *file, int line,
	   unsigned char *fmt, ...);

#undef LOG_ERR
#define LOG_ERR(args...) \
	elinks_log("error", __FILE__, __LINE__, args)

#undef LOG_WARN
#define LOG_WARN(args...) \
	elinks_log("warn", __FILE__, __LINE__, args)

#undef LOG_INFO
#define LOG_INFO(args...) \
	elinks_log("info", __FILE__, __LINE__, args)

#undef LOG_DBG
#define LOG_DBG(args...) \
	elinks_log("debug", __FILE__, __LINE__, args)

#endif
#endif



/** This is our smart assert(). It is basically equivalent to if (x) INTERNAL(),
 * but it generates a uniform message and mainly does not do the test if we are
 * supposed to be lightning fast. Use it, use it a lot! And never forget the
 * recovery path, see below ::if_assert_failed. */

#undef assert
#ifdef CONFIG_FASTMEM
#define assert(x) /* We don't do anything in CONFIG_FASTMEM mode. */
#else
#define assert(x) \
do { if (!assert_failed && (assert_failed = !(x))) { \
	INTERNAL("assertion " #x " failed!"); \
} } while (0)
#endif


/** This is extended assert() version, it can print additional user-specified
 * message. Quite useful not only to detect that _something_ is wrong, but also
 * _how_ wrong is it ;-). Note that the format string must always be a regular
 * string, not a variable reference. Also, be careful _what_ will you attempt
 * to print, or you could easily get just a SIGSEGV instead of the assertion
 * failed message. */

#undef assertm
#ifdef HAVE_VARIADIC_MACROS
#ifdef CONFIG_FASTMEM
#define assertm(x,m...) /* We don't do anything in CONFIG_FASTMEM mode. */
#else
#define assertm(x,m...) \
do { if (!assert_failed && (assert_failed = !(x))) { \
	INTERNAL("assertion " #x " failed: " m); \
} } while (0)
#endif
#else /* HAVE_VARIADIC_MACROS */
#ifdef CONFIG_FASTMEM
#define assertm elinks_assertm
#else
#define assertm errfile = __FILE__, errline = __LINE__, elinks_assertm
#endif
/* This is not nice at all, and does not really work that nice as macros do
 * But it is good to try to do at least _some_ assertm()ing even when the
 * variadic macros are not supported. */
/* XXX: assertm() usage could generate warnings (we assume that the assert()ed
 * expression is int (and that's completely fine, I do *NOT* want to see any
 * stinking assert((int) pointer) ! ;-)), so CONFIG_DEBUG (-Werror) and
 * !HAVE_VARIADIC_MACROS won't play well together. Hrm. --pasky */
#ifdef CONFIG_FASTMEM
static inline
#endif
void elinks_assertm(int x, unsigned char *fmt, ...)
#ifdef CONFIG_FASTMEM
{
	/* We don't do anything in CONFIG_FASTMEM mode. Let's hope that the compiler
	 * will at least optimize out the @x computation. */
}
#else
	;
#endif
#endif /* HAVE_VARIADIC_MACROS */


/** Whether an assertion has failed and the failure has not yet been handled.
 * To make recovery path possible (assertion failed may not mean end of the
 * world, the execution goes on if we're outside of CONFIG_DEBUG and CONFIG_FASTMEM),
 * @c assert_failed is set to true if the last assert() failed, otherwise it's
 * zero. Note that you must never change assert_failed value, sorry guys.
 *
 * You should never test @c assert_failed directly anyway. Use ::if_assert_failed
 * instead, it will attempt to hint compiler to optimize out the recovery path
 * if we're CONFIG_FASTMEM. So it should go like:
 *
 * @code
 * assertm(1 == 1, "The world's gonna blow up!");
 * if_assert_failed { schedule_time_machine(); return; }
 * @endcode
 *
 * In-depth explanation: this restriction is here because in the CONFIG_FASTMEM mode,
 * @c assert_failed is initially initialized to zero and then not ever touched
 * anymore. So if you change it to non-zero failure, your all further recovery
 * paths will get hit (and since developers usually don't test CONFIG_FASTMEM mode
 * extensively...). So better don't mess with it, even if you would do that
 * with awareness of this fact. We don't want to iterate over tens of spots all
 * over the code when we change one detail regarding CONFIG_FASTMEM operation.
 *
 * This is not that actual after introduction of ::if_assert_failed, but it's
 * a safe recommendation anyway, so... ;-) */
extern int assert_failed;

#undef if_assert_failed
#ifdef CONFIG_FASTMEM
#define if_assert_failed if (0) /* This should be optimalized away. */
#else
#define if_assert_failed if (assert_failed && !(assert_failed = 0))
#endif



/** This will print some fancy message, version string and possibly do
 * something else useful. Then, it will dump core. */
#ifdef CONFIG_DEBUG
void force_dump(void);
#endif


/** This function does nothing, except making compiler not to optimize certains
 * spots of code --- this is useful when that particular optimization is buggy.
 * So we are just workarounding buggy compilers.
 *
 * This function should be always used only in context of compiler version
 * specific macros. */
void do_not_optimize_here(void *x);

#if defined(__GNUC__) && __GNUC__ == 2 && __GNUC_MINOR__ <= 7
#define do_not_optimize_here_gcc_2_7(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_2_7(x)
#endif

#if defined(__GNUC__) && __GNUC__ == 3
#define do_not_optimize_here_gcc_3_x(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_3_x(x)
#endif

#if defined(__GNUC__) && __GNUC__ == 3 && __GNUC_MINOR__ == 3
#define do_not_optimize_here_gcc_3_3(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_3_3(x)
#endif


/** This function dumps backtrace (or whatever similar it founds on the stack)
 * nicely formatted and with symbols resolved to @a f. When @a trouble is set,
 * it tells it to be extremely careful and not use dynamic memory allocation
 * functions etc (useful in SIGSEGV handler etc).
 *
 * Note that this function just calls system-specific backend provided by the
 * libc, so it is available only on some systems. CONFIG_BACKTRACE is defined
 * if it is available on yours. */
#ifdef CONFIG_BACKTRACE
#include <stdio.h>
void dump_backtrace(FILE *f, int trouble);
#endif

/** This is needed for providing info about features when dumping core */
extern unsigned char full_static_version[1024];

#endif
