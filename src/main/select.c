/* File descriptors managment and switching */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <string.h> /* FreeBSD FD_ZERO() macro calls bzero() */
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
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

struct thread {
	select_handler_T read_func;
	select_handler_T write_func;
	select_handler_T error_func;
	void *data;
};

static struct thread threads[FD_SETSIZE];

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

	for (j = 0; j < FD_SETSIZE; j++)
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

	bh = mem_alloc(sizeof(*bh));
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
		struct bottom_half *bh = bottom_halves.prev;
		select_handler_T fn = bh->fn;
		void *data = bh->data;

		del_from_list(bh);
		mem_free(bh);
		fn(data);
	}
}

select_handler_T
get_handler(int fd, enum select_handler_type tp)
{
#ifndef CONFIG_OS_WIN32
	assertm(fd >= 0 && fd < FD_SETSIZE,
		"get_handler: handle %d >= FD_SETSIZE %d",
		fd, FD_SETSIZE);
	if_assert_failed return NULL;
#endif
	switch (tp) {
		case SELECT_HANDLER_READ:	return threads[fd].read_func;
		case SELECT_HANDLER_WRITE:	return threads[fd].write_func;
		case SELECT_HANDLER_ERROR:	return threads[fd].error_func;
		case SELECT_HANDLER_DATA:	return threads[fd].data;
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
				      "select()", errno_from_select, (unsigned char *) strerror(errno_from_select));
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
