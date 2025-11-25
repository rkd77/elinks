#!/bin/bash

PREFIX=/opt/elinks
DESTDIR=$HOME/elinks

rm -rf builddir32
export LDFLAGS="-lws2_32"
export CFLAGS="-g2 -O2"
LIBRARY_PATH="$PREFIX/lib" \
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" \
/mingw32/bin/meson setup builddir32 \
-D88-colors=false \
-D256-colors=false \
-Dapidoc=false \
-Dbacktrace=false \
-Dbittorrent=true \
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
-Dspartan=true \
-Dspidermonkey=false \
-Dstatic=false \
-Dterminfo=false \
-Dtest=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=false \
-Dwin32-vt100-native=false \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=true || exit 1

/mingw32/bin/meson compile -C builddir32 || exit 2
/mingw32/bin/meson install -C builddir32 --destdir $DESTDIR || exit 3

# find dlls
cd $DESTDIR/$PREFIX/bin
mv elinks.exe elinks-lite.exe
for i in $(ntldd.exe -R elinks-lite.exe | grep \\\\mingw32\\\\bin | cut -d'>' -f2 | cut -d' ' -f2); do cp -v $i . ; done
cd -
