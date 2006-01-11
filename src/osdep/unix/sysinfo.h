
#ifndef EL__OSDEP_UNIX_SYSINFO_H
#define EL__OSDEP_UNIX_SYSINFO_H

#ifdef CONFIG_OS_UNIX

#define SYSTEM_NAME	"Unix"
#define SYSTEM_STR	"unix"
#define DEFAULT_SHELL	"/bin/sh"
#define GETSHELL	getenv("SHELL")

static inline int dir_sep(char x) { return x == '/'; }

#define FS_UNIX_RIGHTS
#define FS_UNIX_HARDLINKS
#define FS_UNIX_SOFTLINKS
#define FS_UNIX_USERS

#include <pwd.h>
#include <grp.h>

#endif

#endif
