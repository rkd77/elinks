
AC_DEFUN([EL_CONFIG_OS_WIN32],
[
	AC_MSG_CHECKING([for win32 threads])

	EL_SAVE_FLAGS

	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>]], [[_beginthread(NULL, NULL, 0, NULL)]])],[cf_result=yes],[cf_result=no])
	AC_MSG_RESULT($cf_result)

	if test "$cf_result" = yes; then
		EL_DEFINE(HAVE_BEGINTHREAD, [_beginthread()])
	else
		EL_RESTORE_FLAGS
	fi

	AC_CHECK_HEADERS(windows.h ws2tcpip.h)

	# TODO: Check this?
	# TODO: Check -lws2_32 for IPv6 support
	LIBS="$LIBS -lwsock32"
])
