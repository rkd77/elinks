#!/bin/sh

rm -rf /root/tmp/builddir_js

CFLAGS="-O2 -static -no-pie" \
CXXFLAGS="-O2 -static -no-pie" \
meson /root/tmp/builddir_js \
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
-Dquickjs=true \
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

meson compile -j $(($(nproc) - 1)) -C /root/tmp/builddir_js

strip /root/tmp/builddir_js/src/elinks

upx /root/tmp/builddir_js/src/elinks
