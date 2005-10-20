AC_DEFUN([EL_CONFIG_SEE],
[

enable_see="no";

AC_ARG_WITH(see, [  --with-see              enable Simple Ecmascript Engine (SEE) support],
            [ if test "x$withval" != xno; then enable_see=yes; fi ])

# The following is probably bad, ugly and so on. Stolen from Guile's (1.4)
# SEE_FLAGS but I really don't want to require people to have Guile in order
# to compile CVS. Also, the macro seems to be really stupid regarding searching
# for Guile in $PATH etc. --pasky

AC_MSG_CHECKING([for SEE])

if test "$enable_see" = "yes"; then
	AC_MSG_RESULT(yes);
	## Based on the SEE_FLAGS macro.

	if test -d "$withval"; then
		SEE_PATH="$withval:$PATH"
	else
		SEE_PATH="$PATH"
	fi

	AC_PATH_PROG(SEE_CONFIG, libsee-config, no, $SEE_PATH)

	## First, let's just see if we can find Guile at all.
	if test "$SEE_CONFIG" != no; then
		cf_result="yes";

		SEE_LIBS="`$SEE_CONFIG --libs`"
		SEE_CFLAGS="`$SEE_CONFIG --cppflags`"
		LIBS="$SEE_LIBS $LIBS"
		CPPFLAGS="$CPPFLAGS $SEE_CFLAGS"
		EL_CONFIG(CONFIG_SEE, [SEE])
		AC_SUBST(SEE_CFLAGS)
	else
		if test -n "$withval" && test "x$withval" != xno; then
			AC_MSG_ERROR([SEE not found])
		else
			AC_MSG_WARN([SEE support disabled])
		fi
	fi
else
	AC_MSG_RESULT(no);
fi
])
