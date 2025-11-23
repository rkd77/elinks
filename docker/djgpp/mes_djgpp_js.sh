#!/bin/sh

rm -rf /tmp/builddir_js

VER=0.19.0
PREFIX=/opt/elinks
DESTDIR=$HOME/elinks

# -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \
# -DWATT32_NO_NAMESPACE -DWATT32_NO_OLDIES" \

LIBRARY_PATH="$PREFIX/lib" \
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" \
C_INCLUDE_PATH="$PREFIX/include" \
CFLAGS="-O2 -I$PREFIX/include" \
CXXFLAGS="-O2 -I$PREFIX/include" \
LDFLAGS="-L$PREFIX/lib" \
meson setup /tmp/builddir_js --cross-file cross/linux-djgpp.txt \
-D88-colors=false \
-D256-colors=false \
-Dapidoc=false \
-Dbacktrace=false \
-Dbittorrent=false \
-Dbrotli=false \
-Dbzlib=false \
-Dcgi=false \
-Dcss=true \
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
-Dprefix=$PREFIX \
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
-Dzstd=false || exit 1

meson compile -C /tmp/builddir_js || exit 2
mkdir -p $DESTDIR
meson install -C /tmp/builddir_js --destdir $DESTDIR || exit 3

mv -f $DESTDIR/$PREFIX/bin/elinks $DESTDIR/$PREFIX/bin/elinks.exe || exit 4
i586-pc-msdosdjgpp-strip $DESTDIR/$PREFIX/bin/elinks.exe || exit 5
upx $DESTDIR/$PREFIX/bin/elinks.exe || exit 6
