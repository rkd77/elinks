
#ifndef EL__OSDEP_WIN32_SYSINFO_H
#define EL__OSDEP_WIN32_SYSINFO_H

#ifdef CONFIG_OS_WIN32

#define SYSTEM_NAME	"Win32"
#define SYSTEM_STR	"win32"
#define DEFAULT_SHELL	"cmd.exe"
#define GETSHELL	getenv("COMSPEC")

#define dir_sep(x) ((x) == '/' || (x) == '\\')


/* When is this a problem? --jonas */
#if !defined(__CYGWIN__)

#include <io.h>
#include <direct.h>
#include <synchapi.h>

#define mkdir(dir,access)	(mkdir)(dir)
#define sleep(s)		Sleep(1000*(s))
#define wait(status)		_cwait(NULL, 0xdeadbeef, _WAIT_CHILD)

#endif /* __CYGWIN__ */


/*#define NO_ASYNC_LOOKUP*/
/* #define NO_FG_EXEC */
#define DOS_FS
#define NO_FORK_ON_EXIT

/* Misc defines */
#define IN_LOOPBACKNET   127

#ifndef EADDRINUSE
#define EADDRINUSE    WSAEADDRINUSE
#endif

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT    WSAEAFNOSUPPORT
#endif

#ifndef EALREADY
#define EALREADY      WSAEALREADY
#endif

#ifndef ECONNREFUSED
#define ECONNREFUSED  WSAECONNREFUSED
#endif

#ifndef ECONNRESET
#define ECONNRESET    WSAECONNRESET
#endif

#ifndef EINPROGRESS
#define EINPROGRESS   WSAEINPROGRESS
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK   WSAEWOULDBLOCK
#endif

#ifndef ENETUNREACH
#define ENETUNREACH   WSAENETUNREACH
#endif

#endif
#endif
