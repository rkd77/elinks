dnl Thank you very much Vim for this lovely ruby configuration
dnl The hitchhiked code is from Vim configure.in version 1.98


AC_DEFUN([EL_CONFIG_RUBY],
[
AC_MSG_CHECKING([for Ruby])

CONFIG_RUBY="no";

EL_SAVE_FLAGS

AC_ARG_WITH(ruby,
	[  --with-ruby             enable Ruby support],
	[if test "$withval" != no; then CONFIG_RUBY="yes"; fi])

AC_MSG_RESULT($CONFIG_RUBY)

if test "$CONFIG_RUBY" = "yes"; then

	AC_PATH_PROG(CONFIG_RUBY, ruby, no, $withval:$PATH)
	if test "$CONFIG_RUBY" != "no"; then

		AC_MSG_CHECKING(Ruby version)
		if $CONFIG_RUBY -e 'VERSION >= "1.6.0" or exit 1' >/dev/null 2>/dev/null; then
			ruby_version=`$CONFIG_RUBY -e 'puts VERSION'`
			AC_MSG_RESULT($ruby_version)

			AC_MSG_CHECKING(for Ruby header files)
			rubyhdrdir=`$CONFIG_RUBY -r mkmf -e 'print Config::CONFIG[["archdir"]] || $hdrdir' 2>/dev/null`

			if test "X$rubyhdrdir" != "X"; then
				AC_MSG_RESULT($rubyhdrdir)
				RUBY_CFLAGS="-I$rubyhdrdir"
				rubylibs=`$CONFIG_RUBY -r rbconfig -e 'print Config::CONFIG[["LIBS"]]'`

				if test "X$rubylibs" != "X"; then
					RUBY_LIBS="$rubylibs"
				fi

				librubyarg=`$CONFIG_RUBY -r rbconfig -e 'print Config.expand(Config::CONFIG[["LIBRUBYARG"]])'`

				if test -f "$rubyhdrdir/$librubyarg"; then
					librubyarg="$rubyhdrdir/$librubyarg"

				else
					rubylibdir=`$CONFIG_RUBY -r rbconfig -e 'print Config.expand(Config::CONFIG[["libdir"]])'`
					if test -f "$rubylibdir/$librubyarg"; then
						librubyarg="$rubylibdir/$librubyarg"
					elif test "$librubyarg" = "libruby.a"; then
						dnl required on Mac OS 10.3 where libruby.a doesn't exist
						librubyarg="-lruby"
					else
						librubyarg=`$CONFIG_RUBY -r rbconfig -e "print '$librubyarg'.gsub(/-L\./, %'-L#{Config.expand(Config::CONFIG[\"libdir\"])}')"`
					fi
				fi

				if test "X$librubyarg" != "X"; then
					RUBY_LIBS="$librubyarg $RUBY_LIBS"
				fi

				rubyldflags=`$CONFIG_RUBY -r rbconfig -e 'print Config::CONFIG[["LDFLAGS"]]'`
				if test "X$rubyldflags" != "X"; then
					LDFLAGS="$rubyldflags $LDFLAGS"
				fi

				LIBS="$RUBY_LIBS $LIBS"
				CFLAGS="$RUBY_CFLAGS $CFLAGS"
				CPPFLAGS="$CPPFLAGS $RUBY_CFLAGS"

				AC_TRY_LINK([#include <ruby.h>],
					    [ruby_init();],
					    CONFIG_RUBY=yes, CONFIG_RUBY=no)
			else
				AC_MSG_RESULT(not found, disabling Ruby)
			fi
		else
			AC_MSG_RESULT(too old; need Ruby version 1.6.0 or later)
		fi
	fi
fi

if test "$CONFIG_RUBY" != "yes"; then
	EL_RESTORE_FLAGS
else
	EL_CONFIG(CONFIG_RUBY, [Ruby])

	CFLAGS="$CFLAGS_X"
	AC_SUBST(RUBY_CFLAGS)
	AC_SUBST(RUBY_LIBS)
fi
])
