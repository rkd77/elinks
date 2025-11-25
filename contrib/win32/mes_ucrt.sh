#!/bin/bash

# script for Windows64 build using msys2

PREFIX=/opt/elinks
DESTDIR=$HOME/elinks

rm -rf builddir3
export LDFLAGS="-lws2_32"
export CFLAGS="-g2 -O2"
LIBRARY_PATH="$PREFIX/lib" \
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" \
/ucrt64/bin/meson setup builddir3 \
-D88-colors=false \
-D256-colors=false \
-Dapidoc=false \
-Dbacktrace=false \
-Dbittorrent=false \
-Dbrotli=true \
-Dbzlib=false \
-Dcgi=false \
-Dcodepoint=false \
-Dcss=true \
-Ddebug=false \
-Ddgi=true \
-Ddoc=false \
-Dexmode=true \
-Dfastmem=true \
-Dfsp=false \
-Dfsp2=true \
-Dgemini=true \
-Dgettext=true \
-Dgnutls=false \
-Dgopher=true \
-Dgpm=false \
-Dgssapi=false \
-Dguile=false \
-Dhtml-highlight=true \
-Dhtmldoc=false \
-Didn=true \
-Dipv6=true \
-Dlibcss=true \
-Dlibcurl=true \
-Dlibev=false \
-Dlibevent=false \
-Dlibsixel=false \
-Dluapkg= \
-Dlzma=false \
-Dmouse=true \
-Dmujs=false \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dpdfdoc=false \
-Dperl=false \
-Dprefix=$PREFIX \
-Dpython=false \
-Dquickjs=false \
-Dreproducible=false \
-Druby=false \
-Dsm-scripting=false \
-Dsmb=false \
-Dspidermonkey=false \
-Dstatic=false \
-Dterminfo=false \
-Dtest=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=false \
-Dwin32-vt100-native=true \
-Dwithdebug=false \
-Dx=false \
-Dxbel=false \
-Dzlib=true \
-Dzstd=true || exit 1

/ucrt64/bin/meson compile -C builddir3 || exit 2
mkdir -p $HOME/elinks
/ucrt64/bin/meson install -C builddir3 --destdir $DESTDIR || exit 3

# find dlls
cd $DESTDIR/$PREFIX/bin
for i in $(ldd elinks.exe | grep /ucrt64/bin | cut -d' ' -f3); do cp -v $i . ; done
cd -
