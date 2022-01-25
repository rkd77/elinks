/* File descriptors managment and switching */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <string.h> /* FreeBSD FD_ZERO() macro calls bzero() */
#ifdef __GNU__ /* For GNU Hurd bug workaround in set_handlers() */
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* This must be here, thanks to BSD. */
#ifdef HAVE_INTTYPES_H
#include <inttypes.h> /* OMG */
#endif

#if defined(HAVE_POLL_H) && defined(HAVE_POLL) && !defined(INTERIX) && !defined(__HOS_AIX__)
#define USE_POLL
#include <poll.h>
#endif

#if defined(HAVE_LIBEV) && !defined(OPENVMS) && !defined(DOS)
#ifdef HAVE_LIBEV_EVENT_H
#include <libev/event.h>
#elif defined(HAVE_EVENT_H)
#include <event.h>
#endif
#define USE_LIBEVENT
#endif

#if (defined(HAVE_EVENT_H) || defined(HAVE_EV_EVENT_H) || defined(HAVE_LIBEV_EVENT_H)) && defined(HAVE_LIBEVENT) && !defined(OPENVMS) && !defined(DOS)
#if defined(HAVE_EVENT_H)
#include <event.h>
#elif defined(HAVE_EV_EVENT_H)
#include <ev-event.h>
#endif
#define USE_LIBEVENT
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#define EINTRLOOPX(ret_, call_, x_)			\
do {							\
	(ret_) = (call_);				\
} while ((ret_) == (x_) && errno == EINTR)

#define EINTRLOOP(ret_, call_)	EINTRLOOPX(ret_, call_, -1)

#include "elinks.h"

#include "intl/libintl.h"
#include "main/main.h"
#include "main/select.h"
#include "main/timer.h"
#include "osdep/signals.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/time.h"


#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#ifdef USE_LIBEVENT
const char *
get_libevent_version(void)
{
	return event_get_version();
}
#else
const char *
get_libevent_version(void)
{
	return "";
}
#endif

/*
#define DEBUG_CALLS
*/

static int n_threads = 0;

struct thread {
	select_handler_T read_func;
	select_handler_T write_func;
	select_handler_T error_func;
	void *data;
#ifdef USE_LIBEVENT
	struct event *read_event;
	struct event *write_event;
#endif
};

#ifdef CONFIG_OS_WIN32
/* CreatePipe produces big numbers for handles */
#undef FD_SETSIZE
#define FD_SETSIZE 4096
#endif

static struct thread *threads = NULL;

static fd_set w_read;
static fd_set w_write;
static fd_set w_error;

static fd_set x_read;
static fd_set x_write;
static fd_set x_error;

static int w_max;

int
get_file_handles_count(void)
{
	int i = 0, j;

	for (j = 0; j < w_max; j++)
		if (threads[j].read_func
		    || threads[j].write_func
		    || threads[j].error_func)
			i++;
	return i;
}

struct bottom_half {
	LIST_HEAD(struct bottom_half);

	select_handler_T fn;
	void *data;
};

static INIT_LIST_OF(struct bottom_half, bottom_halves);

int
register_bottom_half_do(select_handler_T fn, void *data)
{
	struct bottom_half *bh;

	foreach (bh, bottom_halves)
		if (bh->fn == fn && bh->data == data)
			return 0;

	bh = (struct bottom_half *)mem_alloc(sizeof(*bh));
	if (!bh) return -1;
	bh->fn = fn;
	bh->data = data;
	add_to_list(bottom_halves, bh);

	return 0;
}

void
check_bottom_halves(void)
{
	while (!list_empty(bottom_halves)) {
		struct bottom_half *bh = (struct bottom_half *)bottom_halves.prev;
		select_handler_T fn = bh->fn;
		void *data = bh->data;

		del_from_list(bh);
		mem_free(bh);
		fn(data);
	}
}

#ifdef USE_LIBEVENT

#if defined(USE_POLL)

static void
restrict_fds(void)
{
#if defined(RLIMIT_OFILE) && !defined(RLIMIT_NOFILE)
#define RLIMIT_NOFILE RLIMIT_OFILE
#endif
#if defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit limit;
	int rs;
	EINTRLOOP(rs, getrlimit(RLIMIT_NOFILE, &limit));
	if (rs)
		goto skip_limit;
	if (limit.rlim_cur > FD_SETSIZE) {
		limit.rlim_cur = FD_SETSIZE;
		EINTRLOOP(rs, setrlimit(RLIMIT_NOFILE, &limit));
	}
skip_limit:;
#endif
}
#endif /* USE_POLL */

int event_enabled = 0;

#ifndef HAVE_EVENT_GET_STRUCT_EVENT_SIZE
#define sizeof_struct_event		sizeof(struct event)
#else
#define sizeof_struct_event		(event_get_struct_event_size())
#endif

static inline
struct event *timer_event(struct timer *tm)
{
	return (struct event *)((char *)tm - sizeof_struct_event);
}

#ifdef HAVE_EVENT_BASE_SET
struct event_base *event_base;
#endif

static void
event_callback(int h, short ev, void *data)
{
#ifndef EV_PERSIST
	if (event_add((struct event *)data, NULL) == -1)
		elinks_internal("ERROR: event_add failed: %s", strerror(errno));
#endif
	if (!(ev & EV_READ) == !(ev & EV_WRITE))
		elinks_internal("event_callback: invalid flags %d on handle %d", (int)ev, h);
	if (ev & EV_READ) {
#if defined(HAVE_LIBEV)
		/* Old versions of libev badly interact with fork and fire
		 * events spuriously. */
		if (ev_version_major() < 4 && !can_read(h)) return;
#endif
		threads[h].read_func(threads[h].data);
	} else if (ev & EV_WRITE) {
#if defined(HAVE_LIBEV)
		/* Old versions of libev badly interact with fork and fire
		 * events spuriously. */
		if (ev_version_major() < 4 && !can_write(h)) return;
#endif
		threads[h].write_func(threads[h].data);
	}
	check_bottom_halves();
}

static void
set_event_for_action(int h, void (*func)(void *), struct event **evptr, short evtype)
{
	if (func) {
		if (!*evptr) {
#ifdef EV_PERSIST
			evtype |= EV_PERSIST;
#endif
			*evptr = (struct event *)mem_alloc(sizeof_struct_event);
			event_set(*evptr, h, evtype, event_callback, *evptr);
#ifdef HAVE_EVENT_BASE_SET
			if (event_base_set(event_base, *evptr) == -1)
				elinks_internal("ERROR: event_base_set failed: %s, handle %d", strerror(errno), h);
#endif
		}
		if (event_add(*evptr, NULL) == -1)
			elinks_internal("ERROR: event_add failed: %s, handle %d", strerror(errno), h);
	} else {
		if (*evptr) {
			if (event_del(*evptr) == -1)
				elinks_internal("ERROR: event_del failed: %s, handle %d", strerror(errno), h);
		}
	}
}

static void
set_events_for_handle(int h)
{
	set_event_for_action(h, threads[h].read_func, &threads[h].read_event, EV_READ);
	set_event_for_action(h, threads[h].write_func, &threads[h].write_event, EV_WRITE);
}

static void
enable_libevent(void)
{
	int i;

	if (get_cmd_opt_bool("no-libevent"))
		return;

#if !defined(NO_FORK_ON_EXIT) && defined(HAVE_KQUEUE) && !defined(HAVE_EVENT_REINIT)
	/* kqueue doesn't work after fork */
	if (!F)
		return;
#endif

#if defined(HAVE_EVENT_CONFIG_SET_FLAG)
	{
		struct event_config *cfg;
		cfg = event_config_new();
		if (!cfg)
			return;
		if (event_config_set_flag(cfg, EVENT_BASE_FLAG_NOLOCK) == -1) {
			event_config_free(cfg);
			return;
		}
		event_base = event_base_new_with_config(cfg);
		event_config_free(cfg);
		if (!event_base)
			return;
	}
#elif defined(HAVE_EVENT_BASE_NEW)
	event_base = event_base_new();
	if (!event_base)
		return;
#elif defined(HAVE_EVENT_BASE_SET)
	event_base = event_init();
	if (!event_base)
		return;
#else
	event_init();
#endif
	event_enabled = 1;

	for (i = 0; i < w_max; i++)
		set_events_for_handle(i);

/*
	foreach(tm, timers)
		set_event_for_timer(tm);
*/
	set_events_for_timer();
}

static void
terminate_libevent(void)
{
	int i;
	if (event_enabled) {
		for (i = 0; i < n_threads; i++) {
			set_event_for_action(i, NULL, &threads[i].read_event, EV_READ);
			if (threads[i].read_event)
				mem_free(threads[i].read_event);
			set_event_for_action(i, NULL, &threads[i].write_event, EV_WRITE);
			if (threads[i].write_event)
				mem_free(threads[i].write_event);
		}
#ifdef HAVE_EVENT_BASE_FREE
		event_base_free(event_base);
#endif
		event_enabled = 0;
	}
}

static void
do_event_loop(int flags)
{
	int e;
#ifdef HAVE_EVENT_BASE_SET
	e = event_base_loop(event_base, flags);
#else
	e = event_loop(flags);
#endif
	if (e == -1)
		elinks_internal("ERROR: event_base_loop failed: %s", strerror(errno));
}
#endif


select_handler_T
get_handler(int fd, enum select_handler_type tp)
{
	if (fd >= w_max) {
		return NULL;
	}

	switch (tp) {
		case SELECT_HANDLER_READ:	return threads[fd].read_func;
		case SELECT_HANDLER_WRITE:	return threads[fd].write_func;
		case SELECT_HANDLER_ERROR:	return threads[fd].error_func;
		case SELECT_HANDLER_DATA:	return (select_handler_T)threads[fd].data;
	}

	INTERNAL("get_handler: bad type %d", tp);
	return NULL;
}

void
set_handlers(int fd, select_handler_T read_func, select_handler_T write_func,
	     select_handler_T error_func, void *data)
{
#ifndef CONFIG_OS_WIN32
	assertm(fd >= 0 && fd < FD_SETSIZE,
		"set_handlers: handle %d >= FD_SETSIZE %d",
		fd, FD_SETSIZE);
	if_assert_failed return;
#endif
#ifdef __GNU__
	/* GNU Hurd pflocal bug <http://savannah.gnu.org/bugs/?22861>:
	 * If ELinks does a select() where the initial exceptfds set
	 * includes a pipe that is not listed in the other fd_sets,
	 * then select() always reports an exception there.  That
	 * makes Elinks think the pipe has failed and close it.
	 * To work around this bug, do not monitor exceptions for
	 * pipes on the Hurd.  */
	if (error_func) {
		struct stat st;

		if (fstat(fd, &st) == 0 && S_ISFIFO(st.st_mode))
			error_func = NULL;
	}
#endif /* __GNU__ */

#if defined(USE_POLL) && defined(USE_LIBEVENT)
	if (!event_enabled)
#endif
		if (fd >= (int)FD_SETSIZE) {
			elinks_internal("too big handle %d", fd);
			return;
		}
	if (fd >= n_threads) {
		struct thread *tmp_threads = (struct thread *)mem_realloc(threads, (fd + 1) * sizeof(struct thread));

		if (!tmp_threads) {
			elinks_internal("out of memory");
			return;
		}
		threads = tmp_threads;
		memset(threads + n_threads, 0, (fd + 1 - n_threads) * sizeof(struct thread));
		n_threads = fd + 1;
	}

	if (threads[fd].read_func == read_func && threads[fd].write_func == write_func
	&& threads[fd].error_func == error_func && threads[fd].data == data) {
		return;
	}

	threads[fd].read_func = read_func;
	threads[fd].write_func = write_func;
	threads[fd].error_func = error_func;
	threads[fd].data = data;

	if (read_func || write_func || error_func) {
		if (fd >= w_max) w_max = fd + 1;
	} else if (fd == w_max - 1) {
		int i;

		for (i = fd - 1; i >= 0; i--) {
			if (threads[i].read_func || threads[i].write_func || threads[i].error_func)
				break;
		}
		w_max = i + 1;
	}

#ifdef USE_LIBEVENT
	if (event_enabled) {
		set_events_for_handle(fd);
		return;
	}
#endif
	if (read_func) {
		FD_SET(fd, &w_read);
	} else {
		FD_CLR(fd, &w_read);
		FD_CLR(fd, &x_read);
	}

	if (write_func) {
		FD_SET(fd, &w_write);
	} else {
		FD_CLR(fd, &w_write);
		FD_CLR(fd, &x_write);
	}

	if (error_func) {
		FD_SET(fd, &w_error);
	} else {
		FD_CLR(fd, &w_error);
		FD_CLR(fd, &x_error);
	}
}

void
select_loop(void (*init)(void))
{
	timeval_T last_time;
	int select_errors = 0;

	clear_signal_mask_and_handlers();
	FD_ZERO(&w_read);
	FD_ZERO(&w_write);
	FD_ZERO(&w_error);
	w_max = 0;
	timeval_now(&last_time);
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	init();
	check_bottom_halves();

#ifdef USE_LIBEVENT
	enable_libevent();
#if defined(USE_POLL)
	if (!event_enabled) {
		restrict_fds();
	}
#endif
	if (event_enabled) {
		while (!program.terminate) {
			check_signals();
			if (1 /*(!F)*/) {
				do_event_loop(EVLOOP_NONBLOCK);
				check_signals();
				redraw_all_terminals();
			}
			if (program.terminate) break;
			do_event_loop(EVLOOP_ONCE);
		}
	} else
#endif

	while (!program.terminate) {
		struct timeval *timeout = NULL;
		int n, i, has_timer;
		timeval_T t;

		check_signals();
		check_timers(&last_time);
		redraw_all_terminals();

		memcpy(&x_read, &w_read, sizeof(fd_set));
		memcpy(&x_write, &w_write, sizeof(fd_set));
		memcpy(&x_error, &w_error, sizeof(fd_set));

		if (program.terminate) break;

		has_timer = get_next_timer_time(&t);
		if (!w_max && !has_timer) break;
		critical_section = 1;

		if (check_signals()) {
			critical_section = 0;
			continue;
		}
#if 0
		{
			int i;

			printf("\nR:");
			for (i = 0; i < 256; i++)
				if (FD_ISSET(i, &x_read)) printf("%d,", i);
			printf("\nW:");
			for (i = 0; i < 256; i++)
				if (FD_ISSET(i, &x_write)) printf("%d,", i);
			printf("\nE:");
			for (i = 0; i < 256; i++)
				if (FD_ISSET(i, &x_error)) printf("%d,", i);
			fflush(stdout);
		}
#endif
		if (has_timer) {
			/* Be sure timeout is not negative. */
			timeval_limit_to_zero_or_one(&t);
			timeout = (struct timeval *) &t;
		}

		n = select(w_max, &x_read, &x_write, &x_error, timeout);
		if (n < 0) {
			/* The following calls (especially gettext)
			 * might change errno.  */
			const int errno_from_select = errno;

			critical_section = 0;
			uninstall_alarm();
			if (errno_from_select != EINTR) {
				ERROR(gettext("The call to %s failed: %d (%s)"),
				      "select()", errno_from_select, (char *) strerror(errno_from_select));
				if (++select_errors > 10) /* Infinite loop prevention. */
					INTERNAL(gettext("%d select() failures."),
						 select_errors);
			}
			continue;
		}

		select_errors = 0;
		critical_section = 0;
		uninstall_alarm();
		check_signals();
		/*printf("sel: %d\n", n);*/
		check_timers(&last_time);

		i = -1;
		while (n > 0 && ++i < w_max) {
			int k = 0;

#if 0
			printf("C %d : %d,%d,%d\n", i, FD_ISSET(i, &w_read),
			       FD_ISSET(i, &w_write), FD_ISSET(i, &w_error));
			printf("A %d : %d,%d,%d\n", i, FD_ISSET(i, &x_read),
			       FD_ISSET(i, &x_write), FD_ISSET(i, &x_error));
#endif
			if (FD_ISSET(i, &x_read)) {
				if (threads[i].read_func) {
					threads[i].read_func(threads[i].data);
					check_bottom_halves();
				}
				k = 1;
			}

			if (FD_ISSET(i, &x_write)) {
				if (threads[i].write_func) {
					threads[i].write_func(threads[i].data);
					check_bottom_halves();
				}
				k = 1;
			}

			if (FD_ISSET(i, &x_error)) {
				if (threads[i].error_func) {
					threads[i].error_func(threads[i].data);
					check_bottom_halves();
				}
				k = 1;
			}

			n -= k;
		}
	}
}

static int
can_read_or_write(int fd, int write)
{
#if defined(USE_POLL)
	struct pollfd p;
	int rs;
	p.fd = fd;
	p.events = !write ? POLLIN : POLLOUT;
	EINTRLOOP(rs, poll(&p, 1, 0));
	if (rs < 0) elinks_internal("ERROR: poll for %s (%d) failed: %s", !write ? "read" : "write", fd, strerror(errno));
	if (!rs) return 0;
	if (p.revents & POLLNVAL) elinks_internal("ERROR: poll for %s (%d) failed: %s", !write ? "read" : "write", fd, strerror(errno));
	return 1;
#else
	struct timeval tv = {0, 0};
	fd_set fds;
	fd_set *rfds = NULL;
	fd_set *wfds = NULL;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (write)
		wfds = &fds;
	else
		rfds = &fds;

	return select(fd + 1, rfds, wfds, NULL, &tv);
#endif
}

int
can_read(int fd)
{
	return can_read_or_write(fd, 0);
}

int
can_write(int fd)
{
	return can_read_or_write(fd, 1);
}

void
terminate_select(void)
{
#ifdef USE_LIBEVENT
	terminate_libevent();
#endif
	mem_free_if(threads);
}
