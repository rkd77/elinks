#ifndef EL__MAIN_SELECT_H
#define EL__MAIN_SELECT_H

#if defined(HAVE_LIBEV) && !defined(OPENVMS) && !defined(DOS)
#ifdef HAVE_LIBEV_EVENT_H
#include <libev/event.h>
#elif defined(HAVE_EVENT_H)
#include <event.h>
#endif
#define USE_LIBEVENT
#endif

#if (defined(HAVE_EVENT2_EVENT_H) || defined(HAVE_EV_EVENT_H) || defined(HAVE_LIBEV_EVENT_H)) && defined(HAVE_LIBEVENT) && !defined(OPENVMS) && !defined(DOS)
#if defined(HAVE_EVENT2_EVENT_H)
#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#elif defined(HAVE_EV_EVENT_H)
#include <ev-event.h>
#endif
#define USE_LIBEVENT
#endif

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#include "main/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EINTRLOOPX(ret_, call_, x_)			\
do {							\
	(ret_) = (call_);				\
} while ((ret_) == (x_) && errno == EINTR)

#define EINTRLOOP(ret_, call_)	EINTRLOOPX(ret_, call_, -1)

#ifndef NO_SIGNAL_HANDLERS
extern pid_t signal_pid;
#if defined(HAVE_SYS_EVENTFD_H) && !defined(CONFIG_LIBUV)
extern int signal_efd;
#else
extern int signal_pipe[2];
#endif
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
/* Global information, common to all connections */
typedef struct _GlobalInfo
{
	timer_id_T tim;
	struct event_base *evbase;
	struct event timer_event;
	CURLM *multi;
	int still_running;
	FILE *input;
	int stopped;
} GlobalInfo;

extern GlobalInfo g;

void check_multi_info(GlobalInfo *g);

void mcode_or_die(const char *where, CURLMcode code);
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEV)
/* Global information, common to all connections */
typedef struct _GlobalInfo
{
	timer_id_T tim;
	struct ev_loop *loop;
	struct ev_timer timer_event;
	CURLM *multi;
	int still_running;
	FILE *input;
} GlobalInfo;

extern GlobalInfo g;

void check_multi_info(GlobalInfo *g);

void mcode_or_die(const char *where, CURLMcode code);
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBUV)
struct datauv {
	uv_timer_t timeout;
	uv_loop_t *loop;
	CURLM *multi;
};

struct curl_context {
	uv_poll_t poll_handle;
	curl_socket_t sockfd;
	struct datauv *uv;
};

extern struct datauv g;

void check_multi_info(struct datauv *g);

void mcode_or_die(const char *where, CURLMcode code);
#endif



#if defined(CONFIG_LIBCURL) && !defined(CONFIG_LIBEV) && !defined(CONFIG_LIBEVENT) && !defined(CONFIG_LIBUV)
/* Global information, common to all connections */
typedef struct _GlobalInfo
{
	timer_id_T tim;
	CURLM *multi;
	int still_running;
	FILE *input;
} GlobalInfo;

extern GlobalInfo g;

void check_multi_info(GlobalInfo *g);

void mcode_or_die(const char *where, CURLMcode code);
#endif

typedef void (*select_handler_T)(void *);

/* Start the select loop after calling the passed @init() function. */
void select_loop(void (*init)(void));

/* Get information about the number of descriptors being checked by the select
 * loop. */
int get_file_handles_count(void);

/* Schedule work to be done when appropriate in the future. */
int register_bottom_half_do(select_handler_T work_handler, void *data);

/* Wrapper to remove a lot of casts from users of bottom halves. */
#define register_bottom_half(fn, data) \
	register_bottom_half_do((select_handler_T) (fn), (void *) (data))

/* Check and run scheduled work. */
void check_bottom_halves(void);

enum select_handler_type {
	SELECT_HANDLER_READ,
	SELECT_HANDLER_WRITE,
	SELECT_HANDLER_ERROR,
};

enum el_type_hint {
	EL_TYPE_NONE = 0,
	EL_TYPE_TCP,
	EL_TYPE_UDP,
	EL_TYPE_TTY,
	EL_TYPE_FD,
	EL_TYPE_PIPE,
};

/* Get a registered select handler. */
select_handler_T get_handler(int fd, enum select_handler_type type);

void *get_handler_data(int fd);

/* Set handlers and callback @data for the @fd descriptor. */
void set_handlers(int fd,
		  select_handler_T read_handler,
		  select_handler_T write_handler,
		  select_handler_T error_handler,
		  void *data,
		  enum el_type_hint type_hint);

/* Clear handlers associated with @fd. */
#define clear_handlers(fd) \
	set_handlers(fd, NULL, NULL, NULL, NULL, EL_TYPE_TCP)

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
	uv_handle_t *handle;
	enum el_type_hint type_hint;
#endif
};

extern struct thread *threads;

/* Checks which can be used for querying the read/write state of the @fd
 * descriptor without blocking. The interlink code are the only users. */
int can_read(int fd);
int can_write(int fd);

void terminate_select(void);

const char *get_libevent_version(void);
const char *get_libuv_version(void);

#ifdef CONFIG_LIBUV
struct libuv_priv {
	int fd;
};
#endif

#ifdef __cplusplus
}
#endif

#endif
