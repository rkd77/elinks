#!/bin/sh

rm -rf ~/build

cd ~/elinks

LIBRARY_PATH="$HOME/lib" \
PKG_CONFIG_PATH="$HOME/lib/pkgconfig" \
C_INCLUDE_PATH="$HOME/include" \
CFLAGS="-O0 -I$HOME/include -DCURL_STATICLIB" \
CXXFLAGS="-O0 -I$HOME/include -DCURL_STATICLIB" \
LDFLAGS="-L$HOME/lib -lcrypto -L$HOME/lib -lbrotlicommon -L$HOME/lib -lbrotlidec -L$HOME/lib -lzstd -L$HOME/lib -lssh2 -L$HOME/lib -lnghttp2  -L$HOME/lib -lnghttp3 -L$HOME/lib -lngtcp2 -L$HOME/lib -lngtcp2_crypto_quictls -lws2_32 -lcrypt32 -lwldap32 -lbcrypt -lucrt -liphlpapi" \
meson setup ~/build --cross-file cross/linux-mingw32.txt \
-D88-colors=false \
-D256-colors=false \
-Dapidoc=false \
-Dbacktrace=false \
-Dbittorrent=false \
-Dbrotli=false \
-Dbzlib=false \
-Dcgi=false \
-Dcss=true \
-Dcombining=false \
-Ddgi=true \
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
-Didn=false \
-Dipv6=true \
-Dlibcss=true \
-Dlibcurl=true \
-Dlibev=false \
-Dlibevent=false \
-Dluapkg='lua' \
-Dlzma=false \
-Dmailcap=false \
-Dmouse=true \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dpdfdoc=false \
-Dperl=false \
-Dprefix=$HOME \
-Dpython=false \
-Dquickjs=false \
-Druby=false \
-Dsm-scripting=false \
-Dspidermonkey=false \
-Dstatic=true \
-Dterminfo=false \
-Dtest=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=true \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=false

meson compile -C ~/build
