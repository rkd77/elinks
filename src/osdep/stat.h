/* Stat-related compatibility stuff. */

#ifndef EL__OSDEP_STAT_H
#define EL__OSDEP_STAT_H

#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* File permission flags not available on win32 systems. */
#ifndef S_IRUSR
#define S_IRUSR 0000400                 /* R for user */
#endif
#ifndef S_IWUSR
#define S_IWUSR 0000200                 /* W for user */
#endif
#ifndef S_IXUSR
#define S_IXUSR 0000100                 /* X for user */
#endif
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

#ifndef S_IRWXU
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#ifndef S_IRWXG
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#endif
#ifndef S_IRWXO
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#endif

#endif /* EL__OSDEP_STAT_H */
