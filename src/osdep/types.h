#ifndef EL__OSDEP_TYPES_H
#define EL__OSDEP_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef SHRT_MAX
#define SHRT_MAX 0x7fff
#endif

#ifndef USHRT_MAX
#define USHRT_MAX 0xffff
#endif

#ifndef INT_MAX
#ifdef MAXINT
#define INT_MAX MAXINT
#else
#define INT_MAX 0x7fffffff
#endif
#endif

#ifndef UINT_MAX
#ifdef MAXUINT
#define UINT_MAX MAXUINT
#else
#define UINT_MAX 0xffffffff
#endif
#endif

#ifndef LONG_MAX
#ifdef MAXLONG
#define LONG_MAX MAXLONG
#else
#define LONG_MAX 0x7fffffff
#endif
#endif

#if defined(HAVE_LONG_LONG) && !defined(LLONG_MAX)
#ifdef MAXLLONG
#define LLONG_MAX MAXLLONG
#elif SIZEOF_LONG_LONG == 8
#define LLONG_MAX 9223372036854775807LL
#elif SIZEOF_LONG_LONG == 4
#define LLONG_MAX LONG_MAX
#endif
#endif

#ifndef OFFT_MAX
#if defined(HAVE_LONG_LONG) && SIZEOF_OFF_T == SIZEOF_LONG_LONG
#define OFFT_MAX LLONG_MAX
#elif SIZEOF_OFF_T == SIZEOF_LONG
#define OFFT_MAX LONG_MAX
#elif SIZEOF_OFF_T == SIZEOF_INT
#define OFFT_MAX INT_MAX
#elif SIZEOF_OFF_T == SIZEOF_SHORT
#define OFFT_MAX SHRT_MAX
#endif
#endif

#ifndef HAVE_UINT16_T
#if SIZEOF_CHAR == 2
typedef unsigned char uint16_t;
#elif SIZEOF_SHORT == 2
typedef unsigned short uint16_t;
#elif SIZEOF_INT == 2
typedef unsigned int uint16_t;
#elif SIZEOF_LONG == 2
typedef unsigned long uint16_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 2
typedef unsigned long long uint16_t;
#else
#error You have no 16-bit integer type. Get in touch with reality.
#endif
#endif

#ifndef HAVE_INT32_T
#if SIZEOF_CHAR == 4
typedef char int32_t;
#elif SIZEOF_SHORT == 4
typedef short int32_t;
#elif SIZEOF_INT == 4
typedef int int32_t;
#elif SIZEOF_LONG == 4
typedef long int32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 4
typedef long long int32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

#ifndef HAVE_UINT32_T
#if SIZEOF_CHAR == 4
typedef unsigned char uint32_t;
#elif SIZEOF_SHORT == 4
typedef unsigned short uint32_t;
#elif SIZEOF_INT == 4
typedef unsigned int uint32_t;
#elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
#elif defined(HAVE_LONG_LONG) && SIZEOF_LONG_LONG == 4
typedef unsigned long long uint32_t;
#else
#error You have no 32-bit integer type. Get in touch with reality.
#endif
#endif

#ifdef HAVE_LONG_LONG
#define longlong long long
#else
#define longlong long
#endif

/* These are mostly for shutting up sparse warnings. */
#ifndef __INT_MAX__
#define __INT_MAX__ 0x7fffffff
#endif
#ifndef __LONG_MAX__
#define __LONG_MAX__ 0x7fffffff
#endif
#ifndef __SHRT_MAX__
#define __SHRT_MAX__ 0x7fff
#endif

/*
 * long l; (long) (longptr_T) l == l
 * void *p; (void *) (longptr_T) p == p
 */
typedef long longptr_T;

/* To print off_t offset, ELinks does:
 *
 * printf("%" OFF_PRINT_FORMAT, (off_print_T) offset);
 *
 * The cast is necessary because it is not possible to guess
 * a printf format for off_t itself based on what we have here.
 * The off_t type might be either long or long long, and the format
 * string must match even if both types have the same representation,
 * because GCC warns about mismatches and --enable-debug adds -Werror
 * to $CFLAGS.  */
#if !HAVE_OFF_T || SIZEOF_OFF_T <= SIZEOF_LONG
typedef long off_print_T;
# define OFF_PRINT_FORMAT "ld"
#elif HAVE_LONG_LONG && SIZEOF_OFF_T <= SIZEOF_LONG_LONG
typedef long long off_print_T;
# define OFF_PRINT_FORMAT "lld"
#else
# error "cannot figure out how to print off_t values"
#endif

#endif
