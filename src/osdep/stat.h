/* $Id: stat.h,v 1.1.2.2 2005/09/14 15:42:00 jonas Exp $ */

/* Stat-related compatibility stuff. */

#ifndef EL__OSDEP_STAT_H
#define EL__OSDEP_STAT_H

#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* File permission flags not available on win32 systems. */
#ifndef S_ISUID
#define S_ISUID 0004000                 /* set user id on execution */
#endif
#ifndef S_IRGRP
#define S_IRGRP 0000040                 /* R for group */
#endif
#ifndef S_IWGRP
#define S_IWGRP 0000020                 /* W for group */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0000010                 /* X for group */
#endif
#ifndef S_ISGID
#define S_ISGID 0002000                 /* set group id on execution */
#endif
#ifndef S_IROTH
#define S_IROTH 0000004                 /* R for other */
#endif
#ifndef S_IWOTH
#define S_IWOTH 0000002                 /* W for other */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0000001                 /* X for other */
#endif
#ifndef S_ISVTX
#define S_ISVTX  0001000                /* save swapped text even after use */
#endif

#endif
