#!/bin/sh

echo acinclude.m4...
(
	echo "dnl Automatically generated from config/m4/ files by autogen.sh!"
	echo "dnl Do not modify!"
	cat config/m4/*.m4
) > acinclude.m4

echo aclocal...
aclocal

echo autoheader...
autoheader

echo autoconf...
autoconf

echo config.cache...
rm -f config.cache

echo done
