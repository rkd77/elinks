#!/bin/sh
#
# WIndows x64 elinks cross-compilation
#

./autogen.sh

CC=x86_64-w64-mingw32-gcc \
LD=x86_64-w64-mingw32-ld \
CFLAGS="-g -static -no-pie" \
PKG_CONFIG="./pkg-config.sh" \
./configure -C \
--host=x86_64-w64-mingw32 \
--enable-static \
--without-brotli \
--enable-utf-8 \
--enable-256-colors \
--without-quickjs \
--without-lzma \
--disable-gopher \
--without-bzlib \
--without-zlib \
--disable-backtrace \
--without-openssl \
--disable-debug \
--enable-fastmem \
--without-perl \
--disable-88-colors \
--disable-true-color \
--prefix=/usr \
--disable-combining \
--disable-bittorrent \
--without-gnutls \
--without-libev \
--without-libevent \
--without-terminfo \
--disable-cgi \
--without-ruby \
--disable-sm-scripting \
--without-python \
--without-zstd \
--without-x \
--disable-nls
if [ $? = 0 ]; then
  sed -i 's/-Wall/-w/g' Makefile.config
  make 
else
  print config failed  
fi

