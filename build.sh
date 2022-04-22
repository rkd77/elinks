#!/bin/bash
#
# shell script to build static elinks
# for differenct architectures with
# the same configuration
#

clear

echo '       --/                                    \--'
echo '       --[ Welcome to the elinks build helper ]--'
echo '       --[                                    ]--'
echo '       --[ [*] select the architecture 1-4    ]--'
echo '       --[ [*] use option 5 for config        ]--'
echo '       --[ [*] use option 6 for make          ]--'
echo '       --[ [*] use option 7 for test          ]--'
echo '       --[ [*] use option 8 for publishing    ]--'
echo '       --\                                    /--'
echo '                                                 '

gen_conf() {
  ./autogen.sh
}

configure() {
  echo "--[ Configure starts in 2 seconds ]--"
  echo "--[ Compiler: " $1 "]--"
  echo "--[ Host    : " $2 "]--"
  sleep 2
  rm -f config.cache
  time \
  CC=$1 \
  LD=$2 \
  LIBS=$5 \
  CFLAGS="-g -no-pie -std=c99" \
  LDFLAGS=$4 \
  PKG_CONFIG="./pkg-config.sh" \
  ./configure -C \
  --host=$3 \
  --prefix=/usr \
  --enable-256-colors \
  --enable-fastmem \
  --with-static \
  --enable-utf-8 \
  --without-openssl \
  --without-quickjs \
  --disable-88-colors \
  --disable-backtrace \
  --disable-bittorrent \
  --disable-debug \
  --disable-cgi \
  --disable-combining \
  --disable-gopher \
  --disable-nls \
  --disable-ipv6 \
  --disable-sm-scripting \
  --disable-true-color \
  --without-bzlib \
  --without-brotli \
  --without-gnutls \
  --without-libev \
  --without-libevent \
  --without-lzma \
  --without-perl \
  --without-python \
  --without-ruby \
  --without-terminfo \
  --without-zlib \
  --without-zstd \
  --without-x
  if [ $? -eq 0 ]; then
    echo "--[ Configuration Sucessfull ]--"
    # turn off warnings
    sed -i 's/-Wall/-w/g' Makefile.config
    #sed -i 's/-lpthread/-pthread/g' Makefile.config
    build
  else
    echo "--[ Configuration failed... ]--"
  fi
}

# BUILD FUNCTION
build() {
  echo "--[ Build starts in 2 seconds ]--"
  sleep 2
  time make # --trace
  if [ $? -eq 0 ]; then
    echo "--[ ................ ]--"
    echo "--[ Build Sucessfull ]--"
    echo "--[ All Done.        ]--"
    echo "--[ ................ ]--"
  else
    echo "--[ Build failed... ]--"
  fi
}

test() {
  #
  # very basic to test binary
  # won't core dump
  #
  # for arm32: qemu-arm-static
  # for win64: wine
  #
  ./src/elinks$1 \
  --no-connect \
  --dump \
  ./test/hello.html
}

pub() {
  ls -l ./src/elinks$1
  file ./src/elinks$1
  if [ ! -d ../bin ]; then
    mkdir ../bin
  fi
  cp ./src/elinks$1 ../bin/elinks_$2$1
  echo "--[ File Published to ../bin ]--"
}

info() {
  echo "--[ binary info ]--"
  file ./src/elinks$1
  ls -lh ./src/elinks$1
  ls -l ./src/elinks$1
}

set_arch() {
  if [ "$1" = "lin64" ]; then
    ARCHIT="$1"
    CC="x86_64-linux-gnu-gcc"
    LD="x86_64-linux-gnu-ld"
    MAKE_HOST="x86_64-linux-gnu"
    BIN_SUFFIX=""
    LDFLAGS=""
    LIBS=""
  elif [ "$1" = "win64" ]; then
    ARCHIT="$1"
    CC="x86_64-w64-mingw32-gcc"
    LD="x86_64-w64-mingw32-ld"
    MAKE_HOST="x86_64-w64-mingw32"
    BIN_SUFFIX=".exe"
    LDFLAGS=""
    LIBS=""
  elif [ "$1" = "arm32" ]; then
    ARCHIT="$1"
    CC="arm-linux-gnueabihf-gcc"
    LD="arm-linux-gnueabihf-ld"
    MAKE_HOST="arm-linux-gnu"
    BIN_SUFFIX=""
    LDFLAGS=""
    LIBS="-L../../lib/$ARCHIT"
  elif [ "$1" = "native" ]; then
    ARCHIT="$1"
    CC="gcc"
    LD="ld"
    MAKE_HOST=""
    BIN_SUFFIX=""
    LDFLAGS=""
    LIBS=""
  fi
}

# MAIN LOOP
ARCHIT=""
BIN_SUFFIX=""
ARCHS="lin64 win64 arm32"
CC_SEL="lin64 win64 arm32 native \
config make test \
pub debug \
info \
build_all \
exit"
set_arch native
select SEL in $CC_SEL; do
  if [ "$SEL" = "lin64" ]; then
    set_arch lin64
  elif [ "$SEL" = "win64" ]; then
    set_arch win64
  elif [ "$SEL" = "arm32" ]; then
    set_arch arm32
  elif [ "$SEL" = "native" ]; then
    set_arch native
  elif [ "$SEL" = "make" ]; then
    build
  elif [ "$SEL" = "config" ]; then
    configure "$CC" "$LD" "$MAKE_HOST" "$LDFLAGS" "$LIBS"
  elif [ "$SEL" = "test" ]; then
    test $BIN_SUFFIX
  elif [ "$SEL" = "pub" ]; then
    pub "$BIN_SUFFIX" "$ARCHIT"
  elif [ "$SEL" = "debug" ]; then
    gdb ./src/elinks$BIN_SUFFIX
  elif [ "$SEL" = "info" ]; then
    info "$BIN_SUFFIX"
  elif [ "$SEL" = "build_all" ]; then
    #set -f  # avoid globbing (expansion of *).
    arch_arr=($ARCHS)
    for arch in "${arch_arr[@]}"; do
      echo "--[ Building: $arch ]--"
      set_arch "$arch"
      configure "$CC" "$LD" "$MAKE_HOST" "$LDFLAGS" "$LIBS"
      build
      info "$BIN_SUFFIX"
      pub "$BIN_SUFFIX" "$ARCHIT"
    done
  elif [ "$SEL" = "exit" ]; then
    exit
  fi
  echo "--[ Compiler: " $CC " ]--"
  echo "--[ Host    : " $MAKE_HOST " ]--"
done
