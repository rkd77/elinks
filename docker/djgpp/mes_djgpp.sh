#!/bin/sh

rm -rf /tmp/builddir

cd $HOME/elinks

LIBRARY_PATH="$HOME/lib" \
PKG_CONFIG_PATH="$HOME/lib/pkgconfig" \
C_INCLUDE_PATH="$HOME/include" \
CFLAGS="-O2 -I$HOME/include -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \
CXXFLAGS="-O2 -I$HOME/include -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \
LDFLAGS="-L$HOME/lib" \
meson setup /tmp/builddir --cross-file cross/linux-djgpp.txt \
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
-Dutf-8=false \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=false

meson compile -C /tmp/builddir

i586-pc-msdosdjgpp-strip /tmp/builddir/src/elinks.exe

upx /tmp/builddir/src/elinks.exe

#cp -a /tmp/builddir/src/elinks.exe ~/.dosemu/drive_c/ELINKS/src/
