
AC_DEFUN([EL_CONFIG_OS_OS2],
[
	AC_MSG_CHECKING([for OS/2 threads])

	EL_SAVE_FLAGS
	CFLAGS="$CFLAGS -Zmt"

	AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>]], [[_beginthread(NULL, NULL, 0, NULL)]])],[cf_result=yes],[cf_result=no])
	AC_MSG_RESULT($cf_result)

	if test "$cf_result" = yes; then
		EL_DEFINE(HAVE_BEGINTHREAD, [_beginthread()])
	else
		EL_RESTORE_FLAGS
	fi

	AC_CHECK_FUNC(MouOpen, EL_DEFINE(HAVE_MOUOPEN, [MouOpen()]))
	AC_CHECK_FUNC(_read_kbd, EL_DEFINE(HAVE_READ_KBD, [_read_kbd()]))

	AC_MSG_CHECKING([for XFree for OS/2])

	EL_SAVE_FLAGS

	cf_result=no

	if test -n "$X11ROOT"; then
		CFLAGS="$CFLAGS_X -I$X11ROOT/XFree86/include"
		LIBS="$LIBS_X -L$X11ROOT/XFree86/lib -lxf86_gcc"
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pty.h>]], [[struct winsize win;ptioctl(1, TIOCGWINSZ, &win)]])],[cf_result=yes],[cf_result=no])
		if test "$cf_result" = no; then
			LIBS="$LIBS_X -L$X11ROOT/XFree86/lib -lxf86"
			AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pty.h>]], [[struct winsize win;ptioctl(1, TIOCGWINSZ, &win)]])],[cf_result=yes],[cf_result=no])
		fi
	fi

	if test "$cf_result" != yes; then
		EL_RESTORE_FLAGS
	else
		EL_DEFINE(X2, [XFree under OS/2])
	fi

	AC_MSG_RESULT($cf_result)
])
