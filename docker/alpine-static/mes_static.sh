#!/bin/sh

rm -rf /tmp/builddir

LIBRARY_PATH="/usr/local/lib" \
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" \
LDFLAGS="-L/usr/local/lib" \
CFLAGS="-O2 -I/usr/local/include -static -no-pie" \
CXXFLAGS="-O2 -I/usr/local/include -static -no-pie" \
meson setup /tmp/builddir \
-D88-colors=true \
-D256-colors=true \
-Dbacktrace=false \
-Dbittorrent=true \
-Dbrotli=true \
-Dbzlib=true \
-Dcgi=true \
-Dcss=true \
-Ddgi=true \
-Ddoc=false \
-Dexmode=true \
-Dfastmem=true \
-Dfsp=false \
-Dfsp2=true \
-Dgemini=true \
-Dgettext=false \
-Dgnutls=false \
-Dgopher=true \
-Dgpm=false \
-Dguile=false \
-Dhtmldoc=false \
-Dhtml-highlight=true \
-Didn=true \
-Dipv6=true \
-Dkitty=true \
-Dlibavif=false \
-Dlibcss=true \
-Dlibcurl=true \
-Dlibev=false \
-Dlibevent=true \
-Dlibwebp=true \
-Dluapkg='luajit' \
-Dlzma=true \
-Dmailcap=true \
-Dmouse=true \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dpdfdoc=false \
-Dperl=false \
-Dpython=false \
-Dquickjs=false \
-Druby=false \
-Dsm-scripting=false \
-Dspartan=true \
-Dspidermonkey=false \
-Dstatic=true \
-Dterminfo=true \
-Dtest=false \
-Dtre=true \
-Dtrue-color=true \
-Dutf-8=true \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=true || exit 1

meson compile -C /tmp/builddir || exit 2

mv /tmp/builddir/src/elinks /tmp/builddir/src/elinks-lite || exit 3
strip /tmp/builddir/src/elinks-lite || exit 4
upx /tmp/builddir/src/elinks-lite || exit 5
