/* Inter-instances internal communication socket interface */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* struct timeval */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* Blame BSD for position of this includes. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/home.h"
#include "intl/gettext/libintl.h"
#include "main/interlink.h"
#include "main/select.h"
#include "osdep/osdep.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Testing purpose. Do not remove. */
/* At some point zas added experimental support for interlinking
 * ELinks' over a TCP socket. However, it was removed due to some
 * unsettled security related questions. More info in:
 *
 *	src/config/options.c (1.233 -> 1.234)
 *	src/network/interlink.c (1.54 -> 1.55)
 *
 * The log message:
 *
 * 	Experimental remote mode is now (almost) functionnal. To test define
 * 	ELINKS_REMOTE in setup.h, compile elinks, add a second ip to your
 * 	machine or use a second machine, then launch elinks -listen
 * 	<your_first_ip> on one side, and elinks -remote <your_first_ip> on the
 * 	second side. There is many things to improve, and it may not function as
 * 	you hope it does, but feel free to provide enhancements. Note this
 * 	feature may disappear soon if none is interested in. Please report
 * 	issues/comments/ideas/bugs on #elinks.
 */

/* Set to 1 to make ELinks use a tcp socket bound to loopback address with
 * port defined by ELINKS_PORT (setup.h) for internal communications
 * instead of a socket file, which is the preferred way. */
#if 0
#define CONFIG_TCP_INTERLINK
#endif

/* Common to both AF_UNIX and AF_INET stuff. */
struct socket_info {
	struct sockaddr *addr;
	int size;
	int fd;
};

/* Accepted socket info */
static struct socket_info s_info_accept;

/* Listening socket info */
static struct socket_info s_info_listen;

/* Connect socket info */
static struct socket_info s_info_connect;

/* Type of address requested (for get_address()) */
enum addr_type {
	ADDR_IP_CLIENT,
	ADDR_IP_SERVER,
	ADDR_ANY_SERVER,
};


#ifndef CONFIG_TCP_INTERLINK

/*** Unix file socket for internal communication. ***/

/* Compute socket file path and allocate space for it.
 * It returns 0 on error (in this case, there's no need
 * to free anything).
 * It returns 1 on success. */
static int
get_sun_path(struct string *sun_path)
{
	assert(sun_path);
	if_assert_failed return 0;

	if (!elinks_home) return 0;

	if (!init_string(sun_path)) return 0;

	add_to_string(sun_path, elinks_home);
	add_to_string(sun_path, ELINKS_SOCK_NAME);
	add_long_to_string(sun_path,
			   get_cmd_opt_int("session-ring"));

	return 1;
}

/* @type is ignored here => always local. */
static int
get_address(struct socket_info *info, enum addr_type type)
{
	struct sockaddr_un *addr = NULL;
	int sun_path_freespace;
	struct string path;

	assert(info);
	if_assert_failed return -1;

	if (!get_sun_path(&path)) return -1;

	/* Linux defines that as:
	 * #define UNIX_PATH_MAX   108
	 * struct sockaddr_un {
	 *	sa_family_t sun_family;
	 *	char sun_path[UNIX_PATH_MAX];
	 * };
	 *
	 * UNIX_PATH_MAX is not defined on all systems, so we'll use sizeof().
	 */

	/* Following code may need to be changed if at some time the
	 * struct sockaddr_un mess is fixed. For now, i tried to make it
	 * sure and portable.
	 *
	 * Extract from glibc documentation:
	 * char sun_path[108]
	 * This is the file name to use.
	 * Incomplete: Why is 108 a magic number?
	 * RMS suggests making this a zero-length array and tweaking the example
	 * following to use alloca to allocate an appropriate amount of storage
	 * based on the length of the filename.
	 *
	 * But at this day (2003/06/18) it seems there's no implementation of such
	 * thing.
	 * If it was the case, then following code will always generate an error.
	 * --Zas
	 */

	sun_path_freespace = sizeof(addr->sun_path) - (path.length + 1);
	if (sun_path_freespace < 0) {
		INTERNAL("Socket path name '%s' is too long: %d >= %zu",
			 path.source, path.length, sizeof(addr->sun_path));
		goto free_and_error;
	}

	addr = mem_calloc(1, sizeof(*addr));
	if (!addr) goto free_and_error;

	memcpy(addr->sun_path, path.source, path.length); /* ending '\0' is done by calloc() */
	done_string(&path);

	addr->sun_family = AF_UNIX;

	info->addr = (struct sockaddr *) addr;
	/* The size of the address is the offset of the start of the filename,
	 * plus its length, plus one for the terminating null byte (well, this
	 * last byte may or not be needed it depends of....).
	 * Alternatively we can use SUN_LEN() macro but this one is not always
	 * defined nor always defined in the same way. --Zas */
	info->size = sizeof(*addr) - sun_path_freespace;

	return AF_UNIX;

free_and_error:
	done_string(&path);
	mem_free_if(addr);

	return -1;
}

static int
alloc_address(struct socket_info *info)
{
	struct sockaddr_un *sa;

	assert(info);
	if_assert_failed return 0;

	/* calloc() is safer there. */
	sa = mem_calloc(1, sizeof(*sa));
	if (!sa) return 0;

	info->addr = (struct sockaddr *) sa;
	info->size = sizeof(*sa);

	return 1;
}

static void
unlink_unix(struct sockaddr *addr)
{
	assert(addr);
	if_assert_failed return;

	unlink(((struct sockaddr_un *) addr)->sun_path);
}

#define setsock_reuse_addr(fd)


#else /* CONFIG_TCP_INTERLINK */

/*** TCP socket for internal communication. ***/
/* FIXME: IPv6 support. */

/* These may not be defined in netinet/in.h on some systems. */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         ((struct in_addr) 0x7f000001)
#endif

#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED	5000
#endif

/* @type is not used for now, and is ignored, it will
 * be used in remote mode feature. */
static int
get_address(struct socket_info *info, enum addr_type type)
{
	struct sockaddr_in *sin;
	uint16_t port;

	assert(info);
	if_assert_failed return -1;

	/* Each ring is bind to ELINKS_PORT + ring number. */
	port = ELINKS_PORT + get_cmd_opt_int("session-ring");
	if (port < IPPORT_USERRESERVED)
		return -1; /* Just in case of... */

	sin = mem_calloc(1, sizeof(*sin));
	if (!sin) return -1;

	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	info->addr = (struct sockaddr *) sin;
	info->size = sizeof(*sin);

	return PF_INET;
}

static int
alloc_address(struct socket_info *info)
{
	struct sockaddr_in *sa;

	assert(info);
	if_assert_failed return 0;

	/* calloc() is safer there. */
	sa = mem_calloc(1, sizeof(*sa));
	if (!sa) return 0;

	info->addr = (struct sockaddr *) sa;
	info->size = sizeof(*sa);

	return 1;
}

#if defined(SOL_SOCKET) && defined(SO_REUSEADDR)
static void
setsock_reuse_addr(int fd)
{
	int reuse_addr = 1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		   (void *) &reuse_addr, sizeof(reuse_addr));
}
#else
#define setsock_reuse_addr(fd)
#endif

#define unlink_unix(s)

#endif /* CONFIG_TCP_INTERLINK */

/* Max. number of bind attempts. */
#define MAX_BIND_TRIES			3
/* Base delay in useconds between bind attempts.
 * We will sleep that time on first attempt, then
 * 2 * delay, then 3 * delay .... */
#define BIND_TRIES_DELAY		100000
/* Number of connections in listen backlog. */
#define LISTEN_BACKLOG			100

/* Max. number of connect attempts. */
#define MAX_CONNECT_TRIES		3
/* Base delay in useconds between connect attempts. */
#define CONNECT_TRIES_DELAY		50000

static void
report_af_unix_error(unsigned char *function, int error)
{
	ERROR(gettext("The call to %s failed: %d (%s)"),
	      function, error, (unsigned char *) strerror(error));
}

/* Called when we receive a connection on listening socket. */
static void
af_unix_connection(struct socket_info *info)
{
	int ns;
	int l;

	assert(info);
	if_assert_failed return;

	l = info->size;

	memset(info->addr, 0, l);
	ns = accept(info->fd, info->addr, &l);
	if (ns < 0) {
		report_af_unix_error("accept()", errno);
		return;
	}

	init_term(ns, ns);

	set_highpri();
}

/* usleep() is not portable, so we use this replacement.
 * TODO: move it to somewhere. */
void
elinks_usleep(unsigned long useconds)
{
	struct timeval delay;
	fd_set dummy;

	FD_ZERO(&dummy);

	delay.tv_sec = 0;
	delay.tv_usec = useconds;
	select(0, &dummy, &dummy, &dummy, &delay);
}

/* Listen on socket for internal ELinks communication.
 * Returns -1 on error
 * or listened file descriptor on success. */
static int
bind_to_af_unix(void)
{
	mode_t saved_mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	int attempts = 0;
	int pf = get_address(&s_info_listen, ADDR_IP_SERVER);

	if (pf == -1) goto free_and_error;

	while (1) {
		s_info_listen.fd = socket(pf, SOCK_STREAM, 0);
		if (s_info_listen.fd == -1) {
			report_af_unix_error("socket()", errno);
			goto free_and_error;
		}

		setsock_reuse_addr(s_info_listen.fd);

		if (bind(s_info_listen.fd, s_info_listen.addr, s_info_listen.size) >= 0)
			break;

		if (errno != EADDRINUSE)
			report_af_unix_error("bind()", errno);

		++attempts;

		if (attempts == MAX_BIND_TRIES)
			unlink_unix(s_info_listen.addr);

		if (attempts > MAX_BIND_TRIES)
			goto free_and_error;

		elinks_usleep(BIND_TRIES_DELAY * attempts);
		close(s_info_listen.fd);
	}

	/* Listen and accept. */
	if (!alloc_address(&s_info_accept))
		goto free_and_error;

	s_info_accept.fd = s_info_listen.fd;

	if (listen(s_info_listen.fd, LISTEN_BACKLOG)) {
		report_af_unix_error("listen()", errno);
		goto free_and_error;
	}

	set_handlers(s_info_listen.fd, (void (*)(void *)) af_unix_connection,
		     NULL, NULL, &s_info_accept);

	umask(saved_mask);
	return s_info_listen.fd;

free_and_error:
	done_interlink();
	umask(saved_mask);

	return -1;
}

/* Connect to an listening socket for internal ELinks communication.
 * Returns -1 on error
 * or file descriptor on success. */
static int
connect_to_af_unix(void)
{
	int attempts = 0;
	int pf = get_address(&s_info_connect, ADDR_IP_CLIENT);

	while (pf != -1 && attempts++ < MAX_CONNECT_TRIES) {
		int saved_errno;

		s_info_connect.fd = socket(pf, SOCK_STREAM, 0);
		if (s_info_connect.fd == -1) {
			report_af_unix_error("socket()", errno);
			break;
		}

		if (connect(s_info_connect.fd, s_info_connect.addr,
			    s_info_connect.size) >= 0)
				return s_info_connect.fd;

		saved_errno = errno;
		close(s_info_connect.fd);

		if (saved_errno != ECONNREFUSED && saved_errno != ENOENT) {
			report_af_unix_error("connect()", errno);
			break;
		}

		elinks_usleep(CONNECT_TRIES_DELAY * attempts);
	}

	mem_free_set(&s_info_connect.addr, NULL);
	return -1;
}

static void safe_close(int *fd) {
	if (*fd == -1) return;
	close(*fd);
	*fd = -1;
}

/* Free all allocated memory and close all descriptors if
 * needed. */
void
done_interlink(void)
{
	/* We test for addr != NULL since
	 * if it was not allocated then fd is not
	 * initialized and we don't want to close
	 * fd 0 ;). --Zas */
	if (s_info_listen.addr) {
		safe_close(&s_info_listen.fd);
		unlink_unix(s_info_listen.addr);
		mem_free(s_info_listen.addr);
		s_info_listen.addr = NULL;
	}

	if (s_info_connect.addr) {
		safe_close(&s_info_connect.fd);
		mem_free(s_info_connect.addr);
		s_info_connect.addr = NULL;
	}

	if (s_info_accept.addr) {
		safe_close(&s_info_accept.fd);
		mem_free(s_info_accept.addr);
		s_info_accept.addr = NULL;
	}
}

/* Initialize sockets for internal ELinks communication.
 * If connect succeeds it returns file descriptor,
 * else it tries to bind and listen on a socket, and
 * return -1
 */
int
init_interlink(void)
{
	int fd = connect_to_af_unix();

	if (fd != -1) return fd;

	bind_to_af_unix();
	return -1;
}


#undef MAX_BIND_TRIES
#undef BIND_TRIES_DELAY
#undef LISTEN_BACKLOG
#undef MAX_CONNECT_TRIES
#undef CONNECT_TRIES_DELAY
