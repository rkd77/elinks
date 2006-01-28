#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

# For repeatability, reset the environment to known value.
LANG=C
LC_ALL=C
PAGER=cat
TZ=UTC
export LANG LC_ALL PAGER TZ

# Each test should start with something like this, after copyright notices:
#
# test_description='Description of this test...
# This test checks if command xyzzy does the right thing...
# '
# . "$TEST_LIB

error () {
	echo "* error: $*"
	trap - exit
	exit 1
}

say () {
	echo "* $*"
}

normalize () {
	echo "$@" | sed '
		s/^[ \t]*\[[^]]*\][ \t]*//;
		s/[:., \t][:., \t]*/-/g;
		s/_/-/g;
		# *cough*
		y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/;
		s/[^a-zA-Z0-9-]//g;'
}


test "${test_description}" != "" ||
error "Test script did not set test_description."

while test "$#" -ne 0
do
	case "$1" in
	-d|--d|--de|--deb|--debu|--debug)
		debug=t; shift ;;
	-i|--i|--im|--imm|--imme|--immed|--immedi|--immedia|--immediat|--immediate)
		immediate=t; shift ;;
	-h|--h|--he|--hel|--help)
		echo "$test_description"
		exit 0 ;;
	-v|--v|--ve|--ver|--verb|--verbo|--verbos|--verbose)
		verbose=t; shift ;;
	*)
		break ;;
	esac
done

exec 5>&1
if test "$verbose" = "t"
then
	exec 4>&2 3>&1
else
	exec 4>/dev/null 3>/dev/null
fi

test_failure=0
test_count=0

trap 'echo >&5 "FATAL: Unexpected exit with code $?"; exit 1' exit

# Test the binaries we have just built.  The tests are kept in
# test/ subdirectory and are run in test/trash subdirectory.
export PATH="$(pwd):$PATH"

for dep in $DEPS; do
	test -e "$dep" || error "You haven't built things yet, have you?"
done


# You are not expected to call test_ok_ and test_failure_ directly, use
# the text_expect_* functions instead.

test_ok_ () {
	test_count=$(expr "$test_count" + 1)
	say "  ok $test_count: $@"
}

test_failure_ () {
	test_count=$(expr "$test_count" + 1)
	test_failure=$(expr "$test_failure" + 1);
	say "FAIL $test_count: $1"
	shift
	echo "$@" | sed -e 's/^/	/'
	test "$immediate" = "" || { trap - exit; exit 1; }
}


test_debug () {
	test "$debug" = "" || eval "$1"
}

test_run_ () {
	eval >&3 2>&4 "$1"
	eval_ret="$?"
	return 0
}

test_expect_failure () {
	test "$#" = 2 ||
	error "bug in the test script: not 2 parameters to test-expect-failure"
	say >&3 "expecting failure: $2"
	test_run_ "$2"
	if [ "$?" = 0 -a "$eval_ret" != 0 ]
	then
		test_ok_ "$1"
	else
		test_failure_ "$@"
	fi
}

test_expect_success () {
	test "$#" = 2 ||
	error "bug in the test script: not 2 parameters to test-expect-success"
	say >&3 "expecting success: $2"
	test_run_ "$2"
	if [ "$?" = 0 -a "$eval_ret" = 0 ]
	then
		test_ok_ "$1"
	else
		test_failure_ "$@"
	fi
}

test_expect_code () {
	test "$#" = 3 ||
	error "bug in the test script: not 3 parameters to test-expect-code"
	say >&3 "expecting exit code $1: $3"
	test_run_ "$3"
	if [ "$?" = 0 -a "$eval_ret" = "$1" ]
	then
		test_ok_ "$2"
	else
		test_failure_ "$@"
	fi
}

test_done () {
	trap - exit
	case "$test_failure" in
	0)
		# We could:
		# cd .. && rm -fr trash
		# but that means we forbid any tests that use their own
		# subdirectory from calling test_done without coming back
		# to where they started from.
		# The Makefile provided will clean this test area so
		# we will leave things as they are.

		say "passed all $test_count test(s)"
		exit 0 ;;

	*)
		say "failed $test_failure among $test_count test(s)"
		exit 1 ;;

	esac
}

# Test repository
test=trash
rm -fr "$test"
mkdir "$test"
cd "$test" || error "Cannot setup test environment"
