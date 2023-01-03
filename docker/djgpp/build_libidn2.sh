#!/bin/bash

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

./configure \
--host=i586-pc-msdosdjgpp \
--enable-static=yes \
--enable-shared=no \
--disable-documentation

make -j`nproc`

make install
