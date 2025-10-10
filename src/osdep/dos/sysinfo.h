#ifndef EL__OSDEP_DOS_SYSINFO_H
#define EL__OSDEP_DOS_SYSINFO_H

#ifdef CONFIG_OS_DOS

#define SYSTEM_NAME	"DOS"
#define SYSTEM_STR	"DOS"
#define DEFAULT_SHELL	"command.com"
#define GETSHELL	getenv("COMSPEC")

#define dir_sep(x) ((x) == '/' || (x) == '\\')
#define NO_ASYNC_LOOKUP
/* #define NO_FG_EXEC */
#define DOS_FS
#define NO_FORK_ON_EXIT

#define ELINKS_TEMPNAME_PREFIX "el"

#endif
#endif
