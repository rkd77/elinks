dnl Thank you very much Vim for this lovely ruby configuration
dnl The hitchhiked code is from Vim configure.in version 1.98


AC_DEFUN([EL_CONFIG_SCRIPTING_RUBY],
[
AC_MSG_CHECKING([for Ruby])

CONFIG_SCRIPTING_RUBY_WITHVAL="no"
CONFIG_SCRIPTING_RUBY="no"

EL_SAVE_FLAGS

AC_ARG_WITH(ruby,
	[  --with-ruby             enable Ruby support],
	[CONFIG_SCRIPTING_RUBY_WITHVAL="$withval"])

if test "$CONFIG_SCRIPTING_RUBY_WITHVAL" != no; then
	CONFIG_SCRIPTING_RUBY="yes"
fi

AC_MSG_RESULT($CONFIG_SCRIPTING_RUBY)

if test "$CONFIG_SCRIPTING_RUBY" = "yes"; then
	if test -d "$CONFIG_SCRIPTING_RUBY_WITHVAL"; then
		RUBY_PATH="$CONFIG_SCRIPTING_RUBY_WITHVAL:$PATH"
	else
		RUBY_PATH="$PATH"
	fi

	AC_PATH_PROG(CONFIG_SCRIPTING_RUBY, ruby, no, $RUBY_PATH)
	if test "$CONFIG_SCRIPTING_RUBY" != "no"; then

		AC_MSG_CHECKING(Ruby version)
		if $CONFIG_SCRIPTING_RUBY -e 'exit((VERSION rescue RUBY_VERSION) >= "1.6.0")' >/dev/null 2>/dev/null; then
			ruby_version=`$CONFIG_SCRIPTING_RUBY -e 'puts "#{VERSION rescue RUBY_VERSION}"'`
			AC_MSG_RESULT($ruby_version)

			AC_MSG_CHECKING(for Ruby header files)
			rubyhdrdir=`$CONFIG_SCRIPTING_RUBY -r mkmf -e 'print RbConfig::CONFIG[["rubyhdrdir"]] || RbConfig::CONFIG[["archdir"]] || $hdrdir' 2>/dev/null`

			if test "X$rubyhdrdir" != "X"; then
				AC_MSG_RESULT($rubyhdrdir)
				RUBY_CFLAGS="-I$rubyhdrdir"
				rubyarch=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e 'print RbConfig::CONFIG[["arch"]]'`
				if test -d "$rubyhdrdir/$rubyarch"; then
					RUBY_CFLAGS="$RUBY_CFLAGS -I$rubyhdrdir/$rubyarch"
				fi
				rubylibs=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e 'print RbConfig::CONFIG[["LIBS"]]'`

				if test "X$rubylibs" != "X"; then
					RUBY_LIBS="$rubylibs"
				fi

				librubyarg=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e 'print RbConfig.expand(RbConfig::CONFIG[["LIBRUBYARG"]])'`

				if test -f "$rubyhdrdir/$librubyarg"; then
					librubyarg="$rubyhdrdir/$librubyarg"

				else
					rubylibdir=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e 'print RbConfig.expand(RbConfig::CONFIG[["libdir"]])'`
					if test -f "$rubylibdir/$librubyarg"; then
						librubyarg="$rubylibdir/$librubyarg"
					elif test "$librubyarg" = "libruby.a"; then
						dnl required on Mac OS 10.3 where libruby.a doesn't exist
						librubyarg="-lruby"
					else
						librubyarg=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e "print '$librubyarg'.gsub(/-L\./, %'-L#{RbConfig.expand(RbConfig::CONFIG[\"libdir\"])}')"`
					fi
				fi

				if test "X$librubyarg" != "X"; then
					RUBY_LIBS="$librubyarg $RUBY_LIBS"
				fi

				rubyldflags=`$CONFIG_SCRIPTING_RUBY -r rbconfig -e 'print RbConfig::CONFIG[["LDFLAGS"]]'`
				if test "X$rubyldflags" != "X"; then
					LDFLAGS="$rubyldflags $LDFLAGS"
				fi

				LIBS="$RUBY_LIBS $LIBS"
				CFLAGS="$RUBY_CFLAGS $CFLAGS"
				CPPFLAGS="$CPPFLAGS $RUBY_CFLAGS"

				AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <ruby.h>]], [[ruby_init();]])],[CONFIG_SCRIPTING_RUBY=yes],[CONFIG_SCRIPTING_RUBY=no])
			else
				AC_MSG_RESULT([Ruby header files not found])
			fi
		else
			AC_MSG_RESULT(too old; need Ruby version 1.6.0 or later)
		fi
	fi
	if test "$CONFIG_SCRIPTING_RUBY" = "yes"; then
		AC_MSG_CHECKING([for rb_errinfo])
		AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <ruby.h>]], [[rb_errinfo();]])],have_rb_errinfo="yes",have_rb_errinfo="no")
		AC_MSG_RESULT($have_rb_errinfo)
		if test "$have_rb_errinfo" = "yes"; then
			AC_DEFINE([HAVE_RB_ERRINFO], [1],
				[Define to 1 if you have the `rb_errinfo' function.])
		fi
	fi
fi

EL_RESTORE_FLAGS

if test "$CONFIG_SCRIPTING_RUBY" != "yes"; then
	if test -n "$CONFIG_SCRIPTING_RUBY_WITHVAL" &&
	   test "$CONFIG_SCRIPTING_RUBY_WITHVAL" != no; then
		AC_MSG_ERROR([Ruby not found])
	fi
else
	EL_CONFIG(CONFIG_SCRIPTING_RUBY, [Ruby])

	LIBS="$LIBS $RUBY_LIBS"
	AC_SUBST(RUBY_CFLAGS)
	AC_SUBST(RUBY_LIBS)
fi
])
