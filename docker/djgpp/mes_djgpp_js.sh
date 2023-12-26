#!/bin/sh

rm -rf /tmp/builddir2

cd $HOME/elinks

LIBRARY_PATH="$HOME/lib" \
PKG_CONFIG_PATH="$HOME/lib/pkgconfig" \
C_INCLUDE_PATH="$HOME/include" \
CFLAGS="-O2 -I$HOME/include -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \
CXXFLAGS="-O2 -I$HOME/include -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \
LDFLAGS="-L$HOME/lib" \
meson setup /tmp/builddir2 --cross-file cross/linux-djgpp.txt \
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
-Dmujs=false \
-Dnls=true \
-Dnntp=true \
-Dopenssl=true \
-Dpdfdoc=false \
-Dperl=false \
-Dprefix=$HOME \
-Dpython=false \
-Dquickjs=true \
-Druby=false \
-Dsm-scripting=false \
-Dspidermonkey=false \
-Dstatic=true \
-Dterminfo=false \
-Dtest=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=false \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=false

meson compile -C /tmp/builddir2

i586-pc-msdosdjgpp-strip /tmp/builddir2/src/elinks.exe

upx /tmp/builddir2/src/elinks.exe

#cp -a /tmp/builddir2/src/elinks.exe ~/.dosemu/drive_c/ELINKS/src/
