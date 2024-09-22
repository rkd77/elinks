/** Error handling and debugging stuff
 * @file */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for vasprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#include "elinks.h"

#include "util/error.h"
#include "util/lists.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


char full_static_version[1024] = "ELinks " VERSION_STRING;

static void
er(int bell, int shall_sleep, const char *fmt, va_list params)
{
	if (bell)
#ifdef CONFIG_OS_WIN32
		MessageBeep(MB_ICONEXCLAMATION);
#else
		fputc(7, stderr); /* load and annoying on Windows */
#endif
	vfprintf(stderr, fmt, params);
	fputc('\n', stderr);
	fflush(stderr);
	if (shall_sleep) sleep(1);
}

int errline;
const char *errfile;
const char *errfun;

void
elinks_debug(const char *fmt, ...)
{
	char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf), "DEBUG MESSAGE at %s:%d: %s",
		 errfile, errline, fmt);

	er(0, 0, errbuf, params);

	va_end(params);
}

void
elinks_wdebug(const char *fmt, ...)
{
	char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf), "DEBUG MESSAGE at %s:%d: %s",
		 errfile, errline, fmt);

	er(0, 1, errbuf, params);

	va_end(params);
}

void
elinks_error(const char *fmt, ...)
{
	char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf), "ERROR at %s:%d: %s",
		 errfile, errline, fmt);

	er(1, 1, errbuf, params);

	va_end(params);
}

void
elinks_internal(const char *fmt, ...)
{
	char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf),
		 "\033[1mINTERNAL ERROR\033[0m at %s %d %s: %s",
		 errfile, errline, errfun, fmt);

	er(1, 1, errbuf, params);

	va_end(params);
#ifdef CONFIG_DEBUG_DUMP
	force_dump();
#endif
}


void
usrerror(const char *fmt, ...)
{
	va_list params;

	va_start(params, fmt);

	fputs("ELinks: ", stderr);
	vfprintf(stderr, fmt, params);
	fputc('\n', stderr);
	fflush(stderr);

	va_end(params);
}


int assert_failed = 0;

void
elinks_assertm(int x, const char *fmt, ...)
{
	char *buf = NULL;
	va_list params;

	if (assert_failed) return;
	if (!(assert_failed = !x)) return;

	va_start(params, fmt);
	(void)!vasprintf((char **) &buf, fmt, params);
	va_end(params);
	elinks_internal("assertion failed: %s", buf);
	if (buf) free(buf);
}


#ifdef CONFIG_DEBUG
void
force_dump(void)
{
	fprintf(stderr,
		"\n\033[1m%s\033[0m %s\n", "Forcing core dump!",
		"Man the Lifeboats! Women and children first!\n");
	fputs("But please DO NOT report this as a segfault!!! It is an internal error, not a\n"
	      "normal segfault, there is a huge difference in these for us the developers.\n"
	      "Also, noting the EXACT error you got above is crucial for hunting the problem\n"
	      "down. Thanks, and please get in touch with us.\n",
	      stderr);
#ifndef CONFIG_BACKTRACE
	fputs(full_static_version, stderr);
	fputc('\n', stderr);
#endif
	fflush(stderr);
	raise(SIGSEGV);
}

static FILE *log_file = NULL;

static void
done_log(void)
{
	char errbuf[4096];
	time_t curtime = time(NULL);
	struct tm *loctime = localtime(&curtime);
	int len;

	len = strftime(errbuf, sizeof(errbuf), "%a, %d %b %Y %H:%M:%S %z",
		       loctime);
	errbuf[len] = '\0';

	fprintf(log_file, "[%-5s %-15s %4s]: Log stopped at %s\n",
		"", "", "", errbuf);

	fclose(log_file);
}

void
elinks_log(char *msg, char *file, int line,char*fun,
	   const char *fmt, ...)
{
	static char *log_files = NULL;
	static char *log_msg = NULL;
	char errbuf[4096];
	char tbuf[32];
	va_list params;
	time_t curtime = time(NULL);
	struct tm *loctime = localtime(&curtime);

	if (!log_file) {
		char *log_name;
		int len;

		log_files = getenv("ELINKS_FILES");
		log_msg	  = getenv("ELINKS_MSG");
		log_name  = getenv("ELINKS_LOG");
		log_file  = log_name ? fopen(log_name, "ab") : stderr;

		if (!log_file) return;

		len = strftime(errbuf, sizeof(errbuf), "%a, %d %b %Y %H:%M:%S %z",
			       loctime);
		errbuf[len] = '\0';

		fprintf(log_file, "Log started at %s\n",errbuf);

		atexit(done_log);
	}

	if (log_files && !strstr(log_files, file))
		return;

	if (log_msg && !strstr(log_msg, msg))
		return;

	va_start(params, fmt);

	strftime(tbuf, sizeof(tbuf), "%T", loctime);
	snprintf(errbuf, sizeof(errbuf), "[%s %s %s %d %s]: %s",
		 tbuf, msg, file, line, fun, fmt);

	vfprintf(log_file, errbuf, params);
	fputc('\n', log_file);
	fflush(log_file);

	va_end(params);
}
#endif


void
do_not_optimize_here(void *p)
{
	/* stop GCC optimization - avoid bugs in it */
}


#ifdef CONFIG_BACKTRACE

/* The backtrace corner. */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

void
dump_backtrace(FILE *f, int trouble)
{
	/* If trouble is set, when we get here, we can be in various
	 * interesting situations like inside of the SIGSEGV handler etc. So be
	 * especially careful here.  Dynamic memory allocation may not work
	 * (corrupted stack). A lot of other things may not work too. So better
	 * don't do anything not 100% necessary. */

#ifdef HAVE_EXECINFO_H
	/* glibc way of doing this */

	void *stack[20];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(stack, 20);

	if (trouble) {
		/* Let's hope fileno() is safe. */
		backtrace_symbols_fd(stack, size, fileno(f));
		/* Now out! */
		return;
	}

	strings = backtrace_symbols(stack, size);

	fprintf(f, "Obtained %d stack frames:\n", (int) size);

	for (i = 0; i < size; i++)
		fprintf(f, "[%p] %s\n", stack[i], strings[i]);

	free(strings);
#endif
	fflush(f);
}

#endif
