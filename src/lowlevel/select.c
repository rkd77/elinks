/* File descriptors managment and switching */
/* $Id: select.c,v 1.48.6.3 2005/04/05 21:08:42 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <string.h> /* FreeBSD FD_ZERO() macro calls bzero() */
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/signals.h"
#include "main.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/ttime.h"


#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif


struct thread {
	void (*read_func)(void *);
	void (*write_func)(void *);
	void (*error_func)(void *);
	void *data;
};

struct timer {
	LIST_HEAD(struct timer);

	ttime interval;
	void (*func)(void *);
	void *data;
	int id;
};

static INIT_LIST_HEAD(timers);
static struct thread threads[FD_SETSIZE];

static fd_set w_read;
static fd_set w_write;
static fd_set w_error;

static fd_set x_read;
static fd_set x_write;
static fd_set x_error;

static int w_max;
static int timer_id = 0;



long
select_info(int type)
{
	int i = 0, j;
	struct timer *timer;

	switch (type) {
		case INFO_FILES:
			for (j = 0; j < FD_SETSIZE; j++)
				if (threads[j].read_func
				    || threads[j].write_func
				    || threads[j].error_func)
					i++;
			return i;
		case INFO_TIMERS:
			foreach (timer, timers) i++;
			return i;
	}

	return 0;
}

struct bottom_half {
	LIST_HEAD(struct bottom_half);

	void (*fn)(void *);
	void *data;
};

INIT_LIST_HEAD(bottom_halves);

int
register_bottom_half(void (*fn)(void *), void *data)
{
	struct bottom_half *bh;

	foreach (bh, bottom_halves)
		if (bh->fn == fn && bh->data == data)
			return 0;

	bh = mem_alloc(sizeof(*bh));
	if (!bh) return -1;
	bh->fn = fn;
	bh->data = data;
	add_to_list(bottom_halves, bh);

	return 0;
}

void
do_check_bottom_halves(void)
{
	do {
		struct bottom_half *bh = bottom_halves.prev;
		void (*fn)(void *) = bh->fn;
		void *data = bh->data;

		del_from_list(bh);
		mem_free(bh);
		fn(data);
	} while (!list_empty(bottom_halves));
}

static ttime last_time;

static void
check_timers(void)
{
	ttime now = get_time();
	ttime interval = now - last_time;
	struct timer *timer;

	foreach (timer, timers) timer->interval -= interval;

	while (!list_empty(timers)) {
		timer = timers.next;

		if (timer->interval > 0) break;

		del_from_list(timer);
		timer->func(timer->data);
		mem_free(timer);
		check_bottom_halves();
	}

	last_time = now;
}

int
install_timer(ttime t, void (*func)(void *), void *data)
{
	struct timer *tm, *tt;

	tm = mem_alloc(sizeof(*tm));
	if (!tm) return -1;
	tm->interval = t;
	tm->func = func;
	tm->data = data;
	tm->id = timer_id++;
	foreach (tt, timers)
		if (tt->interval >= t)
			break;
	add_at_pos(tt->prev, tm);

	return tm->id;
}

void
kill_timer(int id)
{
	struct timer *tm;
	int k = 0;

	foreach (tm, timers) if (tm->id == id) {
		struct timer *tt = tm;

		tm = tm->prev;
		del_from_list(tt);
		mem_free(tt);
		k++;
	}

	assertm(k, "trying to kill nonexisting timer");
	assertm(k < 2, "more timers with same id");
}

void *
get_handler(int fd, int tp)
{
	assertm(fd >= 0 && fd < FD_SETSIZE,
		"get_handler: handle %d >= FD_SETSIZE %d",
		fd, FD_SETSIZE);
	if_assert_failed return NULL;

	switch (tp) {
		case H_READ:	return threads[fd].read_func;
		case H_WRITE:	return threads[fd].write_func;
		case H_ERROR:	return threads[fd].error_func;
		case H_DATA:	return threads[fd].data;
	}

	INTERNAL("get_handler: bad type %d", tp);
	return NULL;
}

void
set_handlers(int fd, void (*read_func)(void *), void (*write_func)(void *),
	     void (*error_func)(void *), void *data)
{
	assertm(fd >= 0 && fd < FD_SETSIZE,
		"set_handlers: handle %d >= FD_SETSIZE %d",
		fd, FD_SETSIZE);
	if_assert_failed return;

	threads[fd].read_func = read_func;
	threads[fd].write_func = write_func;
	threads[fd].error_func = error_func;
	threads[fd].data = data;

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

	if (read_func || write_func || error_func) {
		if (fd >= w_max) w_max = fd + 1;
	} else if (fd == w_max - 1) {
		int i;

		for (i = fd - 1; i >= 0; i--)
			if (FD_ISSET(i, &w_read)
			    || FD_ISSET(i, &w_write)
			    || FD_ISSET(i, &w_error))
				break;
		w_max = i + 1;
	}
}


void
select_loop(void (*init)(void))
{
	int select_errors = 0;

	clear_signal_mask_and_handlers();
	FD_ZERO(&w_read);
	FD_ZERO(&w_write);
	FD_ZERO(&w_error);
	w_max = 0;
	last_time = get_time();
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	init();
	check_bottom_halves();

	while (!terminate) {
		int n, i;
		struct timeval tv;
		struct timeval *tm = NULL;

		check_signals();
		check_timers();
		redraw_all_terminals();

		if (!list_empty(timers)) {
			ttime tt = ((struct timer *) &timers)->next->interval + 1;
			if (tt < 0) tt = 0;
			tv.tv_sec = tt / 1000;
			tv.tv_usec = (tt % 1000) * 1000;
			tm = &tv;
		}

		memcpy(&x_read, &w_read, sizeof(fd_set));
		memcpy(&x_write, &w_write, sizeof(fd_set));
		memcpy(&x_error, &w_error, sizeof(fd_set));

		if (terminate) break;
		if (!w_max && list_empty(timers)) break;
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
		n = select(w_max, &x_read, &x_write, &x_error, tm);
		if (n < 0) {
			critical_section = 0;
			uninstall_alarm();
			if (errno != EINTR) {
				ERROR(gettext("The call to %s failed: %d (%s)"),
				      "select()", errno, (unsigned char *) strerror(errno));
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
		check_timers();

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
