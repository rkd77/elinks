dnl ===================================================================
dnl Macros for various checks
dnl ===================================================================

dnl TODO: Make EL_CONFIG* macros assume CONFIG_* defines so it is possible
dnl to write EL_CONFIG_DEPENDS(SCRIPTING, [GUILE LUA PERL], [...])

dnl EL_CONFIG(define, what)
AC_DEFUN([EL_CONFIG], [
	  $1=yes
	  ABOUT_$1="$2"
	  AC_DEFINE($1, 1, [Define if you want: $2 support])])

dnl EL_LOG_CONFIG(define, description, value)
dnl The first parameter (define) will not be expanded by m4,
dnl and it must be a valid name for a shell variable.
AC_DEFUN([EL_LOG_CONFIG],
[
	about="$2"
	value="$3"
	[msgdots2="`echo $about | sed 's/[0-9]/./g'`"]
	[msgdots1="`echo $msgdots2 | sed 's/[a-z]/./g'`"]
	[msgdots0="`echo $msgdots1 | sed 's/[A-Z]/./g'`"]
	[msgdots="`echo $msgdots0 | sed 's/[-_ ()]/./g'`"]
	DOTS="................................"
	dots=`echo $DOTS | sed "s/$msgdots//"`

	# $msgdots too big?
	if test "$dots" = "$DOTS"; then
		dots=""
	fi

	if test -z "$value"; then
		value="$[$1]"
	fi

	echo "$about $dots $value" >> features.log
	AC_SUBST([$1])
])

dnl EL_CONFIG_DEPENDS(define, CONFIG_* dependencies, what)
AC_DEFUN([EL_CONFIG_DEPENDS],
[
	$1=no
	el_value=

	for dependency in $2; do
		# Hope this is portable?!? --jonas
		eval el_config_value=$`echo $dependency`

		if test "$el_config_value" = yes; then
			el_about_dep=$`echo ABOUT_$dependency`
			eval depvalue=$el_about_dep

			if test -z "$el_value"; then
				el_value="$depvalue"
			else
				el_value="$el_value, $depvalue"
			fi
			$1=yes
		fi
	done

	if test "[$]$1" = yes; then
		EL_CONFIG($1, [$3])
	fi
	EL_LOG_CONFIG([$1], [$3], [$el_value])
])

dnl EL_ARG_ENABLE(define, name, conf-help, arg-help)
AC_DEFUN([EL_ARG_ENABLE],
[
	AC_ARG_ENABLE($2, [$4],
	[
		if test "$enableval" != no; then enableval="yes"; fi
		$1="$enableval";
	])

	if test "x[$]$1" = xyes; then
		EL_CONFIG($1, [$3])
	else
		$1=no
	fi
	EL_LOG_CONFIG([$1], [$3], [])
])

dnl EL_ARG_DEPEND(define, name, depend, conf-help, arg-help)
AC_DEFUN([EL_ARG_DEPEND],
[
	AC_ARG_ENABLE($2, [$5],
	[
		if test "$enableval" != no; then enableval="yes"; fi
		$1="$enableval"
	])

	ENABLE_$1="[$]$1";
	if test "x[$]$1" = xyes; then
		# require all dependencies to be met
		for dependency in $3; do
			el_name=`echo "$dependency" | sed 's/:.*//'`;
			el_arg=`echo "$dependency" | sed 's/.*://'`;
			# Hope this is portable?!? --jonas
			eval el_value=$`echo $el_name`;

			if test "x$el_value" != "x$el_arg"; then
				ENABLE_$1=no;
				break;
			fi
		done

		if test "[$]ENABLE_$1" = yes; then
			EL_CONFIG($1, [$4])
		else
			$1=no;
		fi
	else
		$1=no;
	fi
	EL_LOG_CONFIG([$1], [$4], [])
])

dnl EL_DEFINE(define, what)
AC_DEFUN([EL_DEFINE], [AC_DEFINE($1, 1, [Define if you have $2])])

dnl EL_CHECK_CODE(type, define, includes, code)
AC_DEFUN([EL_CHECK_CODE],
[
	$2=yes;
	AC_MSG_CHECKING([for $1])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[$3]], [[$4]])],[EL_DEFINE($2, [$1])],[$2=no])
	AC_MSG_RESULT([$]$2)
])

dnl EL_CHECK_TYPE(type, default)
AC_DEFUN([EL_CHECK_TYPE],
[
        EL_CHECK_TYPE_LOCAL=yes;
        AC_MSG_CHECKING([for $1])
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
        ]], [[int a = sizeof($1);]])],[EL_CHECK_TYPE_LOCAL=yes],[EL_CHECK_TYPE_LOCAL=no])
        AC_MSG_RESULT([$]EL_CHECK_TYPE_LOCAL)
        if test "x[$]EL_CHECK_TYPE_LOCAL" != "xyes"; then
                AC_DEFINE($1, $2, [Define to $2 if <sys/types.h> doesn't define.])
        fi
])

dnl EL_CHECK_SYS_TYPE(type, define, includes)
AC_DEFUN([EL_CHECK_SYS_TYPE],
[
	EL_CHECK_CODE([$1], [$2], [
#include <sys/types.h>
$3
	], [int a = sizeof($1);])
])

dnl EL_CHECK_NET_TYPE(type, define, include)
AC_DEFUN([EL_CHECK_NET_TYPE],
[
	EL_CHECK_SYS_TYPE([$1], [$2], [
#include<sys/socket.h>
$3
	])
])

dnl EL_CHECK_INT_TYPE(type, define)
AC_DEFUN([EL_CHECK_INT_TYPE],
[
	EL_CHECK_SYS_TYPE([$1], [$2], [
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
	])
])


dnl Save and restore the current build flags

AC_DEFUN([EL_SAVE_FLAGS],
[
	CFLAGS_X="$CFLAGS";
	CPPFLAGS_X="$CPPFLAGS";
	LDFLAGS_X="$LDFLAGS";
	LIBS_X="$LIBS";
])

AC_DEFUN([EL_RESTORE_FLAGS],
[
	CFLAGS="$CFLAGS_X";
	CPPFLAGS="$CPPFLAGS_X";
	LDFLAGS="$LDFLAGS_X";
	LIBS="$LIBS_X";
])

