#!/bin/sh

rm -rf /tmp/builddir2
#meson /tmp/builddir .

LIBRARY_PATH="/usr/local/lib" \
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig" \
C_INCLUDE_PATH="/usr/local/include" \
CFLAGS="-I/usr/local/include -DWATT32_NO_NAMESPACE" \
CXXFLAGS="-I/usr/local/include -DWATT32_NO_NAMESPACE" \
LDFLAGS="-L/usr/local/lib" \
meson /tmp/builddir2 --cross-file cross/linux-djgpp.txt \
-D88-colors=false \
-D256-colors=false \
-Dbacktrace=false \
-Dbittorrent=false \
-Dbrotli=true \
-Dbzlib=true \
-Dcgi=false \
-Dcss=true \
-Dcombining=false \
-Ddgi=true \
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
-Dlibev=false \
-Dlibevent=false \
-Dlzma=true \
-Dmailcap=false \
-Dmouse=true \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dperl=false \
-Dprefix=$HOME \
-Dpython=false \
-Dquickjs=true \
-Druby=false \
-Dsm-scripting=false \
-Dspidermonkey=false \
-Dstatic=true \
-Dterminfo=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=false \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=false \

meson compile -C /tmp/builddir2
