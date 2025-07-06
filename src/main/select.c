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

#if defined(HAVE_POLL_H) && defined(HAVE_POLL) && !defined(INTERIX) && !defined(__HOS_AIX__) && !defined(CONFIG_OS_DOS)
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

#if (defined(HAVE_EVENT2_EVENT_H) || defined(HAVE_EV_EVENT_H)) && defined(HAVE_LIBEVENT) && !defined(OPENVMS) && !defined(DOS)
#if defined(HAVE_EVENT2_EVENT_H)
#include <event2/event.h>
#elif defined(HAVE_EV_EVENT_H)
#include <ev-event.h>
#endif
#define USE_LIBEVENT
#endif

#ifdef CONFIG_LIBUV
#ifdef HAVE_UV_H
#include <uv.h>
#endif
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/main.h"
#include "main/select.h"
#include "main/timer.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "session/download.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memcount.h"
#include "util/memory.h"
#include "util/time.h"


#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#ifndef __STRING
#define __STRING(x) #x
#endif

#undef SOCK_SHIFT
#ifdef CONFIG_OS_WIN32
#define SOCK_SHIFT 1024
#else
#define SOCK_SHIFT 0
#endif

#if defined(CONFIG_LIBEVENT) && defined(CONFIG_LIBCURL)

/* Information associated with a specific easy handle */
typedef struct _ConnInfo
{
	CURL *easy;
	char *url;
	GlobalInfo *global;
	char error[CURL_ERROR_SIZE];
} ConnInfo;

/* Information associated with a specific socket */
typedef struct _SockInfo
{
	curl_socket_t sockfd;
	CURL *easy;
	int action;
	long timeout;
	struct event ev;
	GlobalInfo *global;
} SockInfo;

GlobalInfo g;

#define mycase(code) \
	case code: s = __STRING(code)

/* Die if we get a bad CURLMcode somewhere */
void
mcode_or_die(const char *where, CURLMcode code)
{
	ELOG
	if (CURLM_OK != code) {
		const char *s;

		switch(code) {
		mycase(CURLM_BAD_HANDLE); break;
		mycase(CURLM_BAD_EASY_HANDLE); break;
		mycase(CURLM_OUT_OF_MEMORY); break;
		mycase(CURLM_INTERNAL_ERROR); break;
		mycase(CURLM_UNKNOWN_OPTION); break;
		mycase(CURLM_LAST); break;
		default: s = "CURLM_unknown"; break;
		mycase(CURLM_BAD_SOCKET);
			fprintf(stderr, "ERROR: %s returns %s\n", where, s);
			/* ignore this error */
			return;
		}
		fprintf(stderr, "ERROR: %s returns %s\n", where, s);
		//exit(code);
	}
}

/* Update the event timer after curl_multi library calls */
static int
multi_timer_cb(CURLM *multi, long timeout_ms, GlobalInfo *g)
{
	ELOG
	struct timeval timeout;
	(void)multi;

	timeout.tv_sec = timeout_ms/1000;
	timeout.tv_usec = (timeout_ms%1000)*1000;
	//fprintf(stderr, "multi_timer_cb: Setting timeout to %ld ms\n", timeout_ms);

	/*
	 * if timeout_ms is -1, just delete the timer
	 *
	 * For all other values of timeout_ms, this should set or *update* the timer
	 * to the new value
	 */

	if (timeout_ms == -1) {
		evtimer_del(&g->timer_event);
	} else { /* includes timeout zero */
		evtimer_add(&g->timer_event, &timeout);
	}

	return 0;
}

/* Called by libevent when we get action on a multi socket */
static void
event_cb(int fd, short kind, void *userp)
{
	ELOG
	GlobalInfo *g = (GlobalInfo*) userp;
	CURLMcode rc;

	int action = ((kind & EV_READ) ? CURL_CSELECT_IN : 0) | ((kind & EV_WRITE) ? CURL_CSELECT_OUT : 0);

	rc = curl_multi_socket_action(g->multi, fd, action, &g->still_running);
	mcode_or_die("event_cb: curl_multi_socket_action", rc);
	check_multi_info(g);

#if 0
	if (g->still_running <= 0) {
		//fprintf(stderr, "last transfer done, kill timeout\n");
		if (evtimer_pending(&g->timer_event, NULL)) {
			evtimer_del(&g->timer_event);
		}
	}
#endif
}

/* Called by libevent when our timeout expires */
static void
timer_cb(int fd, short kind, void *userp)
{
	ELOG
	GlobalInfo *g = (GlobalInfo *)userp;
	CURLMcode rc;
	(void)fd;
	(void)kind;

	rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
	mcode_or_die("timer_cb: curl_multi_socket_action", rc);
	check_multi_info(g);
}

/* Clean up the SockInfo structure */
static void
remsock(SockInfo *f)
{
	ELOG
	//fprintf(stderr, "remsock f=%p\n", f);

	if (f) {
		if (event_initialized(&f->ev)) {
			event_del(&f->ev);
		}
		mem_free(f);
	}
}

/* Assign information to a SockInfo structure */
static void
setsock(SockInfo *f, curl_socket_t s, CURL *e, int act, GlobalInfo *g)
{
	ELOG
	int kind = ((act & CURL_POLL_IN) ? EV_READ : 0) | ((act & CURL_POLL_OUT) ? EV_WRITE : 0) | EV_PERSIST;

	f->sockfd = s;
	f->action = act;
	f->easy = e;

	//fprintf(stderr, "setsock f=%p\n", f);


	if (event_initialized(&f->ev)) {
		event_del(&f->ev);
	}
	event_assign(&f->ev, g->evbase, f->sockfd + SOCK_SHIFT, kind, event_cb, g);
	event_add(&f->ev, NULL);
}

/* Initialize a new SockInfo structure */
static void
addsock(curl_socket_t s, CURL *easy, int action, GlobalInfo *g)
{
	ELOG
	//fprintf(stderr, "addsock easy=%p\n", easy);

	SockInfo *fdp = mem_calloc(1, sizeof(SockInfo));
	fdp->global = g;
	setsock(fdp, s, easy, action, g);
	curl_multi_assign(g->multi, s, fdp);
}

/* CURLMOPT_SOCKETFUNCTION */
static int
sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	ELOG
	GlobalInfo *g = (GlobalInfo*) cbp;
	SockInfo *fdp = (SockInfo*) sockp;
//	const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };

	//fprintf(stderr, "socket callback: s=%d e=%p what=%s ", s, e, whatstr[what]);

	if (what == CURL_POLL_REMOVE) {
		//fprintf(stderr, "\n");
		remsock(fdp);
	} else {
		if (!fdp) {
			//fprintf(stderr, "Adding data: %s\n", whatstr[what]);
			addsock(s, e, what, g);
		} else {
			//fprintf(stderr, "Changing action from %s to %s\n", whatstr[fdp->action], whatstr[what]);
			setsock(fdp, s, e, what, g);
		}
	}

	return 0;
}
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEV)
/* Information associated with a specific easy handle */
typedef struct _ConnInfo
{
	ELOG
	CURL *easy;
	char *url;
	GlobalInfo *global;
	char error[CURL_ERROR_SIZE];
} ConnInfo;

/* Information associated with a specific socket */
typedef struct _SockInfo
{
	ELOG
	curl_socket_t sockfd;
	CURL *easy;
	int action;
	long timeout;
	struct ev_io ev;
	int evset;
	GlobalInfo *global;
} SockInfo;

GlobalInfo g;

static void timer_cb(EV_P_ struct ev_timer *w, int revents);

/* Update the event timer after curl_multi library calls */
static int
multi_timer_cb(CURLM *multi, long timeout_ms, GlobalInfo *g)
{
	ELOG
	//DPRINT("%s %li\n", __PRETTY_FUNCTION__,  timeout_ms);
	ev_timer_stop(g->loop, &g->timer_event);

	if (timeout_ms >= 0) {
		/* -1 means delete, other values are timeout times in milliseconds */
		double t = timeout_ms / 1000;

		ev_timer_init(&g->timer_event, timer_cb, t, 0.);
		ev_timer_start(g->loop, &g->timer_event);
	}

	return 0;
}

/* Die if we get a bad CURLMcode somewhere */
void
mcode_or_die(const char *where, CURLMcode code)
{
	ELOG
	if (CURLM_OK != code) {
		const char *s;
		switch(code) {
		case CURLM_BAD_HANDLE:
			s = "CURLM_BAD_HANDLE";
			break;
		case CURLM_BAD_EASY_HANDLE:
			s = "CURLM_BAD_EASY_HANDLE";
			break;
		case CURLM_OUT_OF_MEMORY:
			s = "CURLM_OUT_OF_MEMORY";
			break;
		case CURLM_INTERNAL_ERROR:
			s = "CURLM_INTERNAL_ERROR";
			break;
		case CURLM_UNKNOWN_OPTION:
			s = "CURLM_UNKNOWN_OPTION";
			break;
		case CURLM_LAST:
			s = "CURLM_LAST";
			break;
		default:
			s = "CURLM_unknown";
			break;
		case CURLM_BAD_SOCKET:
			s = "CURLM_BAD_SOCKET";
			fprintf(stderr, "ERROR: %s returns %s\n", where, s);
			/* ignore this error */
			return;
		}
		fprintf(stderr, "ERROR: %s returns %s\n", where, s);
		//exit(code);
	}
}

/* Called by libevent when we get action on a multi socket */
static void
event_cb(EV_P_ struct ev_io *w, int revents)
{
	ELOG
	//DPRINT("%s  w %p revents %i\n", __PRETTY_FUNCTION__, w, revents);
	GlobalInfo *g = (GlobalInfo*) w->data;
	CURLMcode rc;
	int action = ((revents & EV_READ) ? CURL_POLL_IN : 0) | ((revents & EV_WRITE) ? CURL_POLL_OUT : 0);

	rc = curl_multi_socket_action(g->multi, w->fd, action, &g->still_running);
	mcode_or_die("event_cb: curl_multi_socket_action", rc);
	check_multi_info(g);

#if 0
	if (g->still_running <= 0) {
		fprintf(MSG_OUT, "last transfer done, kill timeout\n");
		ev_timer_stop(g->loop, &g->timer_event);
	}
#endif
}

/* Called by libevent when our timeout expires */
static void
timer_cb(EV_P_ struct ev_timer *w, int revents)
{
	ELOG
	//DPRINT("%s  w %p revents %i\n", __PRETTY_FUNCTION__, w, revents);
	GlobalInfo *g = (GlobalInfo *)w->data;
	CURLMcode rc;

	rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
	mcode_or_die("timer_cb: curl_multi_socket_action", rc);
	check_multi_info(g);
}

/* Clean up the SockInfo structure */
static void
remsock(SockInfo *f, GlobalInfo *g)
{
	ELOG
	//printf("%s  \n", __PRETTY_FUNCTION__);

	if (f) {
		if (f->evset) {
			ev_io_stop(g->loop, &f->ev);
		}
		free(f);
	}
}

/* Assign information to a SockInfo structure */
static void
setsock(SockInfo *f, curl_socket_t s, CURL *e, int act, GlobalInfo *g)
{
	ELOG
	//printf("%s  \n", __PRETTY_FUNCTION__);

	int kind = ((act & CURL_POLL_IN) ? EV_READ : 0) | ((act & CURL_POLL_OUT) ? EV_WRITE : 0);

	f->sockfd = s;
	f->action = act;
	f->easy = e;

	if (f->evset) {
		ev_io_stop(g->loop, &f->ev);
	}
	ev_io_init(&f->ev, event_cb, f->sockfd, kind);
	f->ev.data = g;
	f->evset = 1;
	ev_io_start(g->loop, &f->ev);
}

/* Initialize a new SockInfo structure */
static void
addsock(curl_socket_t s, CURL *easy, int action, GlobalInfo *g)
{
	ELOG
	SockInfo *fdp = calloc(1, sizeof(SockInfo));

	fdp->global = g;
	setsock(fdp, s, easy, action, g);
	curl_multi_assign(g->multi, s, fdp);
}

/* CURLMOPT_SOCKETFUNCTION */
static int
sock_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	ELOG
	//DPRINT("%s e %p s %i what %i cbp %p sockp %p\n", __PRETTY_FUNCTION__, e, s, what, cbp, sockp);
	GlobalInfo *g = (GlobalInfo*) cbp;
	SockInfo *fdp = (SockInfo*) sockp;
	//const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE"};

	//fprintf(MSG_OUT, "socket callback: s=%d e=%p what=%s ", s, e, whatstr[what]);

	if (what == CURL_POLL_REMOVE) {
		//fprintf(MSG_OUT, "\n");
		remsock(fdp, g);
	} else {
		if (!fdp) {
			//fprintf(MSG_OUT, "Adding data: %s\n", whatstr[what]);
			addsock(s, e, what, g);
		} else {
			//fprintf(MSG_OUT, "Changing action from %s to %s\n", whatstr[fdp->action], whatstr[what]);
			setsock(fdp, s, e, what, g);
		}
	}

	return 0;
}
#endif

#if !defined(CONFIG_LIBEVENT) && !defined(CONFIG_LIBEV) && defined(CONFIG_LIBCURL)

/* Information associated with a specific easy handle */
typedef struct _ConnInfo
{
	ELOG
	CURL *easy;
	char *url;
	GlobalInfo *global;
	char error[CURL_ERROR_SIZE];
} ConnInfo;

/* Information associated with a specific socket */
typedef struct _SockInfo
{
	ELOG
	curl_socket_t sockfd;
	CURL *easy;
	int action;
	long timeout;
	GlobalInfo *global;
} SockInfo;

GlobalInfo g;

#define mycase(code) \
	case code: s = __STRING(code)

/* Die if we get a bad CURLMcode somewhere */
void
mcode_or_die(const char *where, CURLMcode code)
{
	ELOG
	if (CURLM_OK != code) {
		const char *s;

		switch(code) {
		mycase(CURLM_BAD_HANDLE); break;
		mycase(CURLM_BAD_EASY_HANDLE); break;
		mycase(CURLM_OUT_OF_MEMORY); break;
		mycase(CURLM_INTERNAL_ERROR); break;
		mycase(CURLM_UNKNOWN_OPTION); break;
		mycase(CURLM_LAST); break;
		default: s = "CURLM_unknown"; break;
		mycase(CURLM_BAD_SOCKET);
			fprintf(stderr, "ERROR: %s returns %s\n", where, s);
			/* ignore this error */
			return;
		}
		fprintf(stderr, "ERROR: %s returns %s\n", where, s);
		//exit(code);
	}
}

#endif

#ifdef CONFIG_LIBCURL

/* Called by libevent when our timeout expires */
static void
timer_cb_select(void *userp)
{
	ELOG
	GlobalInfo *g = (GlobalInfo *)userp;
	CURLMcode rc;

	rc = curl_multi_socket_action(g->multi, CURL_SOCKET_TIMEOUT, 0, &g->still_running);
	mcode_or_die("timer_cb: curl_multi_socket_action", rc);
	check_multi_info(g);
}

/* Update the event timer after curl_multi library calls */
static int
multi_timer_cb_select(CURLM *multi, long timeout_ms, GlobalInfo *g)
{
	ELOG
	(void)multi;
	//fprintf(stderr, "multi_timer_cb: Setting timeout to %ld ms\n", timeout_ms);

	/*
	 * if timeout_ms is -1, just let the timer to expire.
	 *
	 * For all other values of timeout_ms, this should set or *update* the timer
	 * to the new value
	 */

	if (timeout_ms != -1) {
		install_timer(&g->tim, timeout_ms, timer_cb_select, g);
	}

	return 0;
}

static void
event_read_cb_select(void *userp)
{
	ELOG
	SockInfo *f = (SockInfo *)userp;
	GlobalInfo *g = (GlobalInfo*)f->global;
	CURLMcode rc;

	int action = CURL_CSELECT_IN;

	rc = curl_multi_socket_action(g->multi, f->sockfd, action, &g->still_running);
	mcode_or_die("event_cb: curl_multi_socket_action", rc);
	check_multi_info(g);
}

static void
event_write_cb_select(void *userp)
{
	ELOG
	SockInfo *f = (SockInfo *)userp;
	GlobalInfo *g = (GlobalInfo*)f->global;
	CURLMcode rc;

	int action = CURL_CSELECT_OUT;

	rc = curl_multi_socket_action(g->multi, f->sockfd, action, &g->still_running);
	mcode_or_die("event_cb: curl_multi_socket_action", rc);
	check_multi_info(g);
}

/* Clean up the SockInfo structure */
static void
remsock_select(SockInfo *f)
{
	ELOG
	//fprintf(stderr, "remsock f=%p\n", f);
	if (f) {
		if (f->sockfd) {
			set_handlers(f->sockfd + SOCK_SHIFT, NULL, NULL, NULL, NULL);
		}
		mem_free(f);
	}
}

/* Assign information to a SockInfo structure */
static void
setsock_select(SockInfo *f, curl_socket_t s, CURL *e, int act, GlobalInfo *g)
{
	ELOG
	int in = act & CURL_POLL_IN;
	int out = act & CURL_POLL_OUT;

	f->sockfd = s;
	f->action = act;
	f->easy = e;

	set_handlers(s + SOCK_SHIFT, in ? event_read_cb_select : NULL, out ? event_write_cb_select : NULL, NULL, f);
}

/* Initialize a new SockInfo structure */
static void
addsock_select(curl_socket_t s, CURL *easy, int action, GlobalInfo *g)
{
	ELOG
	//fprintf(stderr, "addsock easy=%p\n", easy);

	SockInfo *fdp = mem_calloc(1, sizeof(SockInfo));
	fdp->global = g;
	setsock_select(fdp, s, easy, action, g);
	curl_multi_assign(g->multi, s, fdp);
}

/* CURLMOPT_SOCKETFUNCTION */
static int
sock_cb_select(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	ELOG
	GlobalInfo *g = (GlobalInfo*) cbp;
	SockInfo *fdp = (SockInfo*) sockp;
	//const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };

	//fprintf(stderr, "socket callback: s=%d e=%p what=%s ", s, e, whatstr[what]);

	if (what == CURL_POLL_REMOVE) {
		//fprintf(stderr, "\n");
		remsock_select(fdp);
	} else {
		if (!fdp) {
			//fprintf(stderr, "Adding data: %s\n", whatstr[what]);
			addsock_select(s, e, what, g);
		} else {
			//fprintf(stderr, "Changing action from %s to %s\n", whatstr[fdp->action], whatstr[what]);
			setsock_select(fdp, s, e, what, g);
		}
	}

	return 0;
}
#endif

#ifdef USE_LIBEVENT
const char *
get_libevent_version(void)
{
	ELOG
	return event_get_version();
}
#else
const char *
get_libevent_version(void)
{
	ELOG
	return "";
}
#endif

#ifdef CONFIG_LIBUV
const char *
get_libuv_version(void)
{
	ELOG
	return uv_version_string();
}
#else
const char *
get_libuv_version(void)
{
	ELOG
	return "";
}
#endif

/*
#define DEBUG_CALLS
*/

static int n_threads = 0;

#ifdef CONFIG_LIBUV
struct libuv_priv {
	int fd;
};

#endif

struct thread {
	select_handler_T read_func;
	select_handler_T write_func;
	select_handler_T error_func;
	void *data;
#ifdef USE_LIBEVENT
	struct event *read_event;
	struct event *write_event;
#endif
#ifdef CONFIG_LIBUV
	uv_poll_t *handle;
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
	ELOG
	int i = 0, j;

	for (j = 0; j < w_max; j++)
		if (threads[j].read_func
		    || threads[j].write_func
		    || threads[j].error_func)
			i++;
	return i;
}

struct bottom_half {
	LIST_HEAD_EL(struct bottom_half);

	select_handler_T fn;
	void *data;
};

static INIT_LIST_OF(struct bottom_half, bottom_halves);

int
register_bottom_half_do(select_handler_T fn, void *data)
{
	ELOG
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
	ELOG
	while (!list_empty(bottom_halves)) {
		struct bottom_half *bh = (struct bottom_half *)bottom_halves.prev;
		select_handler_T fn = bh->fn;
		void *data = bh->data;

		del_from_list(bh);
		mem_free(bh);
		fn(data);
	}
}

#ifdef CONFIG_LIBUV
int event_enabled = 0;

static void
one_cb(uv_poll_t *handle, int status, int events)
{
	ELOG
	struct libuv_priv *priv = (struct libuv_priv *)uv_handle_get_data((uv_handle_t *)handle);

	if (!priv) {
		return;
	}
	int fd = priv->fd;

	if (events & UV_READABLE) {
		select_handler_T hr = get_handler(fd, SELECT_HANDLER_READ);

		if (hr) {
			hr(get_handler_data(fd));
		}
	}

	if (events & UV_WRITABLE) {
		select_handler_T hw = get_handler(fd, SELECT_HANDLER_WRITE);

		if (hw) {
			hw(get_handler_data(fd));
		}
	}
}

static void
set_events_for_handle(int h)
{
	ELOG
	struct libuv_priv *priv = NULL;
	uv_poll_t *handle = NULL;

	if (!threads[h].read_func && !threads[h].write_func) {
		if (!threads[h].handle) {
			return;
		}
		handle = threads[h].handle;
		uv_poll_stop(handle);
		priv = (struct libuv_priv *)uv_handle_get_data((uv_handle_t *)handle);
		mem_free_if(priv);
		mem_free_set(&threads[h].handle, NULL);
		return;
	}
	if (!threads[h].handle) {
		priv = mem_calloc(1, sizeof(*priv));

		if (!priv) {
			return;
		}
		handle = mem_calloc(1, sizeof(*handle));
		if (!handle) {
			mem_free(priv);
			return;
		}
		uv_poll_init(uv_default_loop(), handle, h);
		threads[h].handle = handle;
	} else {
		handle = threads[h].handle;
		priv = (struct libuv_priv *)uv_handle_get_data((uv_handle_t *)handle);
	}
	priv->fd = h;
	uv_handle_set_data((uv_handle_t *)handle, priv);

	if (threads[h].read_func) {
		uv_poll_start(handle, UV_READABLE, one_cb);
	}

	if (threads[h].write_func) {
		uv_poll_start(handle, UV_WRITABLE, one_cb);
	}
}

static void
enable_libevent(void)
{
	ELOG
	int i;

	if (get_cmd_opt_bool("no-libevent"))
		return;

	event_enabled = 1;

	for (i = 0; i < w_max; i++) {
		set_events_for_handle(i);
	}
	set_events_for_timer();
}

#undef EVLOOP_ONCE
#undef EVLOOP_NONBLOCK
#define EVLOOP_ONCE UV_RUN_ONCE
#define EVLOOP_NONBLOCK UV_RUN_NOWAIT

static void
do_event_loop(int flags)
{
	ELOG
	uv_run(uv_default_loop(), flags);
}
#endif

#if defined(USE_POLL)

static void
restrict_fds(void)
{
	ELOG
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

#ifdef USE_LIBEVENT

int event_enabled = 0;

#ifndef HAVE_EVENT_GET_STRUCT_EVENT_SIZE
#define sizeof_struct_event		sizeof(struct event)
#else
#define sizeof_struct_event		(event_get_struct_event_size())
#endif

#ifdef HAVE_EVENT_BASE_SET
struct event_base *event_base;
#endif

static void
event_callback(int h, short ev, void *data)
{
	ELOG
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
	ELOG
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
	ELOG
	set_event_for_action(h, threads[h].read_func, &threads[h].read_event, EV_READ);
	set_event_for_action(h, threads[h].write_func, &threads[h].write_event, EV_WRITE);
}

static void
enable_libevent(void)
{
	ELOG
	int i;

	if (get_cmd_opt_bool("no-libevent"))
		return;

#if !defined(NO_FORK_ON_EXIT) && defined(HAVE_KQUEUE) && !defined(HAVE_EVENT_REINIT)
	/* kqueue doesn't work after fork */
	if (!F)
		return;
#endif

#if !defined(EVENT__DISABLE_MM_REPLACEMENT) && defined(CONFIG_MEMCOUNT)
	event_set_mem_functions(el_libevent_malloc, el_libevent_realloc, el_libevent_free);
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
	ELOG
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
	ELOG
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
	ELOG
	if (fd >= w_max || fd < 0) {
		return NULL;
	}

	switch (tp) {
		case SELECT_HANDLER_READ:	return threads[fd].read_func;
		case SELECT_HANDLER_WRITE:	return threads[fd].write_func;
		case SELECT_HANDLER_ERROR:	return threads[fd].error_func;
	}

	INTERNAL("get_handler: bad type %d", tp);
	return NULL;
}

void *
get_handler_data(int fd)
{
	ELOG
	if (fd >= w_max || fd < 0) {
		return NULL;
	}

	return threads[fd].data;
}

void
set_handlers(int fd, select_handler_T read_func, select_handler_T write_func,
	     select_handler_T error_func, void *data)
{
	ELOG
	if (fd < 0) {
		return;
	}
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

#if defined(USE_POLL) && (defined(USE_LIBEVENT) || defined(CONFIG_LIBUV))
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

#if defined(USE_LIBEVENT) || defined(CONFIG_LIBUV)
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

static timer_id_T periodic_redraw_timer = TIMER_ID_UNDEF;
static int was_installed_timer = 0;

static void
periodic_redraw_all_terminals(void *data)
{
	ELOG
	redraw_all_terminals();
	was_installed_timer = 0;
}

static void
try_redraw_all_terminals(void)
{
	ELOG
	if (was_installed_timer) {
		return;
	}

	if (are_there_downloads()) {
		install_timer(&periodic_redraw_timer, DISPLAY_TIME_REFRESH, periodic_redraw_all_terminals, NULL);
		was_installed_timer = 1;
	}
	redraw_all_terminals();
}

#ifndef NO_SIGNAL_HANDLERS
static void
clear_events(int h, int blocking)
{
	ELOG
#ifdef HAVE_SYS_EVENTFD_H
	if (1) {
		int rd;
		uint64_t t;
		EINTRLOOP(rd, (int)read(h, &t, sizeof(t)));
	}
#else
#if !defined(O_NONBLOCK) && !defined(FIONBIO)
	blocking = 1;
#endif
	while (blocking ? can_read(h) : 1) {
		unsigned char c[64];
		int rd;
		EINTRLOOP(rd, (int)read(h, c, sizeof c));
		if (rd != sizeof c) break;
	}
#endif
}

pid_t signal_pid;
#ifdef HAVE_SYS_EVENTFD_H
int signal_efd = -1;
#else
int signal_pipe[2] = { -1, -1 };
#endif

static void
clear_events_ptr(void *handle)
{
	ELOG
	clear_events((int)(intptr_t)handle, 0);
}
#endif

void
select_loop(void (*init)(void))
{
	ELOG
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
#if !defined(NO_SIGNAL_HANDLERS)
	signal_pid = getpid();

#ifdef HAVE_SYS_EVENTFD_H
	if ((signal_efd = eventfd(0, EFD_NONBLOCK)) < 0) {
		elinks_internal("ERROR: can't create eventfd for signal handling");
	}
	set_handlers(signal_efd, clear_events_ptr, NULL, NULL, (void *)(intptr_t)signal_efd);
#else
	if (c_pipe(signal_pipe)) {
		elinks_internal("ERROR: can't create pipe for signal handling");
	}
	set_nonblocking_fd(signal_pipe[0]);
	set_nonblocking_fd(signal_pipe[1]);
	set_handlers(signal_pipe[0], clear_events_ptr, NULL, NULL, (void *)(intptr_t)signal_pipe[0]);
#endif
#endif
	init();

#if defined(USE_LIBEVENT) || defined(CONFIG_LIBUV)
	enable_libevent();
#if defined(USE_POLL)
	if (!event_enabled) {
		restrict_fds();
	}
#endif
#endif
#if defined(USE_LIBEVENT) || defined(CONFIG_LIBUV)
	if (event_enabled) {
#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
		memset(&g, 0, sizeof(GlobalInfo));
		g.evbase = event_base;
#ifdef CONFIG_MEMCOUNT
		curl_global_init_mem(CURL_GLOBAL_DEFAULT, el_curl_malloc, el_curl_free, el_curl_realloc, el_curl_strdup, el_curl_calloc);
#else
		curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
		g.multi = curl_multi_init();

//fprintf(stderr, "multi_init\n");

		evtimer_assign(&g.timer_event, g.evbase, timer_cb, &g);

		/* setup the generic multi interface options we want */
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETDATA, &g);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERDATA, &g);

		/* we do not call any curl_multi_socket*() function yet as we have no handles added! */
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEV)
		memset(&g, 0, sizeof(GlobalInfo));
		g.loop = ev_default_loop(0);
#ifdef CONFIG_MEMCOUNT
		curl_global_init_mem(CURL_GLOBAL_DEFAULT, el_curl_malloc, el_curl_free, el_curl_realloc, el_curl_strdup, el_curl_calloc);
#else
		curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
		g.multi = curl_multi_init();

//fprintf(stderr, "multi_init\n");
		ev_timer_init(&g.timer_event, timer_cb, 0., 0.);
		g.timer_event.data = &g;

		/* setup the generic multi interface options we want */
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETDATA, &g);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERDATA, &g);

		/* we do not call any curl_multi_socket*() function yet as we have no handles added! */
#endif
		check_bottom_halves();
		while (!program.terminate) {
			check_signals();
			if (1 /*(!F)*/) {
				do_event_loop(EVLOOP_NONBLOCK);
				check_signals();
				try_redraw_all_terminals();
			}
			if (program.terminate) break;
			do_event_loop(EVLOOP_ONCE);
		}
		if (was_installed_timer) {
			kill_timer(&periodic_redraw_timer);
		}

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
		event_del(&g.timer_event);
		//event_base_free(g.evbase);
		curl_multi_cleanup(g.multi);
		curl_global_cleanup();
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEV)
		//ev_loop(g.loop, 0);
		//event_base_free(g.evbase);
		curl_multi_cleanup(g.multi);
		curl_global_cleanup();
#endif
		return;
	} else
#endif
	{
#if defined(CONFIG_LIBCURL)
		memset(&g, 0, sizeof(GlobalInfo));
#ifdef CONFIG_MEMCOUNT
		curl_global_init_mem(CURL_GLOBAL_DEFAULT, el_curl_malloc, el_curl_free, el_curl_realloc, el_curl_strdup, el_curl_calloc);
#else
		curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
		g.multi = curl_multi_init();

		/* setup the generic multi interface options we want */
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETFUNCTION, sock_cb_select);
		curl_multi_setopt(g.multi, CURLMOPT_SOCKETDATA, &g);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb_select);
		curl_multi_setopt(g.multi, CURLMOPT_TIMERDATA, &g);
		/* we do not call any curl_multi_socket*() function yet as we have no handles added! */
#endif
		check_bottom_halves();

	while (!program.terminate) {
		struct timeval timeout = { 0, 0 };
		struct timeval *timeout_ptr = NULL;
		int n, i, has_timer;
		timeval_T t;

		check_signals();
		check_timers(&last_time);
		try_redraw_all_terminals();

		memcpy(&x_read, &w_read, sizeof(fd_set));
		memcpy(&x_write, &w_write, sizeof(fd_set));
		memcpy(&x_error, &w_error, sizeof(fd_set));

		if (program.terminate) break;

		has_timer = get_next_timer_time(&t);
		if (!w_max && !has_timer) break;

		if (check_signals()) {
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
			timeout.tv_sec = t.sec;
			timeout.tv_usec = t.usec;
			timeout_ptr = &timeout;
		}

		n = loop_select(w_max, &x_read, &x_write, &x_error, timeout_ptr);
		if (n < 0) {
			/* The following calls (especially gettext)
			 * might change errno.  */
			const int errno_from_select = errno;

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
#if defined(CONFIG_LIBCURL)
		curl_multi_cleanup(g.multi);
		curl_global_cleanup();
#endif
	}
	if (was_installed_timer) {
		kill_timer(&periodic_redraw_timer);
	}
}

static int
can_read_or_write(int fd, int write)
{
	ELOG
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

	return select2(fd + 1, rfds, wfds, NULL, &tv);
#endif
}

int
can_read(int fd)
{
	ELOG
	return can_read_or_write(fd, 0);
}

int
can_write(int fd)
{
	ELOG
	return can_read_or_write(fd, 1);
}

void
terminate_select(void)
{
	ELOG
#ifdef USE_LIBEVENT
	terminate_libevent();
#endif
	mem_free_if(threads);
}
