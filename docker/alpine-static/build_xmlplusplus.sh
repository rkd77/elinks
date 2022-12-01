#!/bin/sh

NOCONFIGURE=1 ./autogen.sh

./configure \
--enable-static=yes \
--enable-shared=no \
--disable-documentation

make -j12

make install
