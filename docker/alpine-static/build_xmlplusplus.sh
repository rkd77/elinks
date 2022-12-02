#!/bin/sh

meson \
	-Db_lto=false \
	-Dbuild-tests=false \
	-Dbuild-examples=false \
	-Dbuild-documentation=false \
	-Ddefault_library='static' \
	. output

meson compile -C output

meson install -C output
