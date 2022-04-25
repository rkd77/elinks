#!/bin/bash
#
# shell script menus for elinks binaries building
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
  # Thanks rkd77 for discovery of jemmaloc needed
  # to correct openssl functionality
  # LIBS="-ljemalloc -lpthread -lm"  \
  time \
  CC=$1 \
  LD=$2 \
  LDFLAGS=$4 \
  LIBS=$5 \
  CXXFLAGS=$6 \
  PKG_CONFIG="./pkg-config.sh" \
  ./configure -C \
  --host=$3 \
  --prefix=/usr \
  --enable-256-colors \
  --enable-fastmem \
  --enable-utf-8 \
  --with-static \
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
    #build
    return 0
  else
    echo "--[ Listing errors in config.log ]--"
    cat config.log | grep error | tail
    echo "--[ Configuration failed... ]--"
    return 1
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
    return 0
  else
    echo "--[ Build failed... ]--"
    return 1
  fi
}

test() {
  #
  # very basic to test binary
  # won't core dump
  #
  # for arm32: qemu-arm-static
  # for win64: wineconsole
  #
  #./src/elinks$1 \
  #--no-connect \
  #--dump \
  #./test/hello.html
  # more complete testing
  ./test.sh "$BIN_SUFFIX" "$ARCHIT"
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
  if [ ! -f ../src/elinks$1 ]; then
    file ./src/elinks$1
    ls -lh ./src/elinks$1
    ls -l ./src/elinks$1
    if [ "$ARCHIT" = "win64" ] || [ "$ARCHIT" = "win32" ]; then
      wineconsole --backend=ncurses ./src/elinks$1 --version
    else
      ./src/elinks$1 --version
    fi
  else
    echo "--[*] No binary compiled."
  fi
}

set_arch() {
  if [ "$1" = "lin32" ]; then
    ARCHIT="$1"
    CC="i686-linux-gnu-gcc"
    LD="i686-linux-gnu-ld"
    MAKE_HOST="i686-linux-gnu"
    BIN_SUFFIX=""
    CXXFLAGS=""
    LDFLAGS=""
    LIBS=""
  elif [ "$1" = "lin64" ]; then
    ARCHIT="$1"
    CC="x86_64-linux-gnu-gcc"
    LD="x86_64-linux-gnu-ld"
    MAKE_HOST="x86_64-linux-gnu"
    BIN_SUFFIX=""
    CXXFLAGS=""
    LDFLAGS=""
    LIBS=""
  elif [ "$1" = "win32" ]; then
    ARCHIT="$1"
    CC="i686-w64-mingw32-gcc"
    LD="i686-w64-mingw32-ld"
    MAKE_HOST="x86_64-w32-mingw32"
    BIN_SUFFIX=".exe"
    CXXFLAGS=""
    LDFLAGS=""
    LIBS=""
  elif [ "$1" = "win64" ]; then
    ARCHIT="$1"
    CC="x86_64-w64-mingw32-gcc"
    LD="x86_64-w64-mingw32-ld"
    MAKE_HOST="x86_64-w64-mingw32"
    BIN_SUFFIX=".exe"
    CXXFLAGS="-I/usr/local/include"
    LDFLAGS=""
    LIBS="-lws2_32"
  elif [ "$1" = "arm32" ]; then
    ARCHIT="$1"
    CC="arm-linux-gnueabihf-gcc"
    LD="arm-linux-gnueabihf-ld"
    MAKE_HOST="arm-linux-gnu"
    BIN_SUFFIX=""
    CXXFLAGS=""
    LDFLAGS=""
    LIBS="-L../../lib/$ARCHIT"
  elif [ "$1" = "arm64" ]; then
    ARCHIT="$1"
    CC="aarch64-linux-gnu-gcc"
    LD="aarch64-linux-gnu-ld"
    MAKE_HOST="aarch64-linux-gnu"
    BIN_SUFFIX=""
    CXXFLAGS=""
    LDFLAGS=""
    LIBS="-L../../lib/$ARCHIT"
  elif [ "$1" = "native" ]; then
    ARCHIT="$1"
    CC="gcc"
    LD="ld"
    MAKE_HOST=""
    BIN_SUFFIX=""
    CXXFLAGS=""
    LDFLAGS=""
    LIBS=""
  fi
}

# ARCH SELECTION MENU
arch_menu() {
  MENU_ARCHS="$ARCHS null null null null return"
  echo "[=] Build architecture selection menu"
  select SEL in $MENU_ARCHS; do
    echo "[=] Build architecture selection menu"
    if [ "$SEL" = "lin32" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "lin64" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "win64" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "win32" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "arm32" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "arm64" ]; then
      set_arch "$SEL"
    elif [ "$SEL" = "native" ]; then
      set_arch native
    elif [ "$SEL" = "make" ]; then
      build
    elif [ "$SEL" = "null" ]; then
      echo "[.] This option is intentially left blank"
    elif [ "$SEL" = "return" ]; then
      break
    fi
    echo "--[ Architecture : " $ARCHIT " ]--"
    echo "--[ Compiler     : " $CC " ]--"
    echo "--[ Host         : " $MAKE_HOST " ]--"
  done
}

# MAIN LOOP
ARCHIT=""
BIN_SUFFIX=""
ARCHS="lin32 lin64 win32 win64 arm32 arm64 native"
CC_SEL="arch null null build \
config make test \
pub debug \
info \
build_all \
exit"
set_arch native
select SEL in $CC_SEL; do
  if [ "$SEL" = "arch" ]; then
    arch_menu
  elif [ "$SEL" = "build" ]; then
    configure "$CC" "$LD" "$MAKE_HOST" "$LDFLAGS" "$LIBS" "$CXXFLAGS"
    if [ $? -eq 1 ]; then
      break
    fi
    build
    if [ $? -eq 1 ]; then
      break
    fi
  elif [ "$SEL" = "make" ]; then
    build
  elif [ "$SEL" = "config" ]; then
    configure "$CC" "$LD" "$MAKE_HOST" "$LDFLAGS" "$LIBS" "$CXXFLAGS"
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
      configure "$CC" "$LD" "$MAKE_HOST" "$LDFLAGS" "$LIBS" "$CXXFLAGS"
      if [ $? -eq 1 ]; then
        break
      fi
      build
      if [ $? -eq 1 ]; then
        break
      fi
      info "$BIN_SUFFIX"
      pub "$BIN_SUFFIX" "$ARCHIT"
    done
  elif [ "$SEL" = "null" ]; then
    echo "[.] This option is intentially left blank"
  elif [ "$SEL" = "exit" ]; then
    exit
  fi
  echo "--[ [=] elinks build system main menu ]--"
  echo "--[ Architecture : " $ARCHIT " ]--"
  echo "--[ Compiler     : " $CC " ]--"
  echo "--[ Host         : " $MAKE_HOST " ]--"
done
