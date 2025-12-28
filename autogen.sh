#!/bin/sh
set -e
echo aclocal...
aclocal -I config/m4

echo autoheader...
autoheader
# The timestamp of stamp-h.in indicates when config.h.in was last
# generated or found to be already correct.  Create stamp-h.in so
# that it gets included in elinks-*.tar.gz and Makefile won't try
# to run a possibly incompatible version of autoheader (bug 936).
echo timestamp > stamp-h.in

echo autoconf...
autoconf

echo config.cache, autom4te.cache...
rm -f config.cache
rm -rf autom4te.cache

echo done
