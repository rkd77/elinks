/* This is... er, the OS-independent part of osdep/ ;-). */

#ifndef EL__OSDEP_GENERIC_H
#define EL__OSDEP_GENERIC_H

#ifdef HAVE_STDALIGN_H
#include <stdalign.h>
#endif

#include <limits.h> /* may contain PIPE_BUF definition on some systems */

#include <stddef.h> /* may contain offsetof() */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PIPE_BUF
#define PIPE_BUF	512 /* POSIX says that. -- Mikulas */
#endif

/* These are not available on some IRIX systems. */
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifdef CONFIG_IPV6
#define IP_ADDRESS_BUFFER_SIZE INET6_ADDRSTRLEN
#else
#define IP_ADDRESS_BUFFER_SIZE INET_ADDRSTRLEN
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif

#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif

/* Attempt to workaround the EINTR mess. */
#if defined(EINTR) && !defined(CONFIG_OS_DOS)

#if defined(TEMP_FAILURE_RETRY) && !defined(CONFIG_LIBUV)	/* GNU libc */
#define safe_read(fd, buf, count) TEMP_FAILURE_RETRY(read(fd, buf, count))
#define safe_write(fd, buf, count) TEMP_FAILURE_RETRY(write(fd, buf, count))
#else /* TEMP_FAILURE_RETRY */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static inline ssize_t
safe_read(int fd, void *buf, size_t count) {
	do {
		ssize_t r = read(fd, buf, count);

		if (r == -1 && (errno == EINTR || errno == EAGAIN)) continue;
		return r;
	} while (1);
}

static inline ssize_t
safe_write(int fd, const void *buf, size_t count) {
	do {
		ssize_t w = write(fd, buf, count);

		if (w == -1 && (errno == EINTR || errno == EAGAIN)) continue;
		return w;
	} while (1);
}
#endif /* TEMP_FAILURE_RETRY */

#else /* EINTR && !CONFIG_OS_WIN32 */

#ifdef CONFIG_OS_DOS
int dos_read(int fd, void *buf, size_t size);
int dos_write(int fd, const void *buf, size_t size);
#define safe_read(fd, buf, count) dos_read(fd, buf, count)
#define safe_write(fd, buf, count) dos_write(fd, buf, count)

#else

#define safe_read(fd, buf, count) read(fd, buf, count)
#define safe_write(fd, buf, count) write(fd, buf, count)

#endif

#endif /* EINTR && !CONFIG_OS_WIN32 */

#ifndef HAVE_FTELLO
#define ftello(stream) ftell(stream)
#endif

#ifndef HAVE_FSEEKO
#define fseeko(stream, offset, whence) fseek(stream, offset, whence)
#endif

/* Compiler area: */

/* Some compilers, like SunOS4 cc, don't have offsetof in <stddef.h>.  */
#ifndef offsetof
#define offsetof(type, ident) ((size_t) &(((type *) 0)->ident))
#endif

#if !defined(alignof) && ((!defined(__cplusplus) || __cplusplus < 201103L))
/* Alignment of types.  */
#define alignof(TYPE) offsetof(struct { unsigned char dummy1; TYPE dummy2; }, dummy2)
#endif

/* Using this macro to copy structs is both faster and safer than
 * memcpy(destination, source, sizeof(source)). Please, use this macro instead
 * of memcpy(). */
#define copy_struct(destination, source) \
	do { (*(destination) = *(source)); } while (0)

#define sizeof_array(array) (sizeof(array)/sizeof(*(array)))

#ifdef __cplusplus
}
#endif

#endif
