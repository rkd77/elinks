#!/bin/sh

rm -rf /tmp/builddir_js

LIBRARY_PATH="$HOME/lib" \
PKG_CONFIG_PATH="$HOME/lib/pkgconfig" \
LDFLAGS="-L$HOME/lib" \
CFLAGS="-O2 -I$HOME/include -DCURL_STATICLIB -static -no-pie" \
CXXFLAGS="-O2 -I$HOME/include -DCURL_STATICLIB -static -no-pie" \
meson setup /tmp/builddir_js \
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
-Dquickjs=true \
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

meson compile -C /tmp/builddir_js || exit 2
strip /tmp/builddir_js/src/elinks || exit 3
upx /tmp/builddir_js/src/elinks || exit 4
