#!/bin/sh

rm -rf /root/tmp/builddir

LIBRARY_PATH="/usr/local/lib" \
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" \
LDFLAGS="-L/usr/local/lib" \
CFLAGS="-O2 -I/usr/local/include -static -no-pie" \
CXXFLAGS="-O2 -I/usr/local/include -static -no-pie" \
meson /root/tmp/builddir \
-D88-colors=true \
-D256-colors=true \
-Dbacktrace=false \
-Dbittorrent=true \
-Dbrotli=true \
-Dbzlib=true \
-Dcgi=true \
-Dcss=true \
-Dcombining=false \
-Ddgi=true \
-Dexmode=true \
-Dfastmem=true \
-Dfsp=false \
-Dgemini=true \
-Dgettext=false \
-Dgnutls=false \
-Dgopher=true \
-Dgpm=false \
-Dguile=false \
-Didn=true \
-Dipv6=true \
-Dlibcss=true \
-Dlibcurl=true \
-Dlibev=false \
-Dlibevent=true \
-Dluapkg='luajit' \
-Dlzma=true \
-Dmailcap=true \
-Dmouse=true \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dperl=false \
-Dpython=false \
-Dquickjs=false \
-Druby=false \
-Dsm-scripting=false \
-Dspidermonkey=false \
-Dstatic=true \
-Dterminfo=false \
-Dtest=false \
-Dtre=true \
-Dtrue-color=true \
-Dutf-8=true \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=true

meson compile -C /root/tmp/builddir

#strip /root/tmp/builddir/src/elinks

#upx /root/tmp/builddir/src/elinks
