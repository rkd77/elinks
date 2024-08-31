#!/bin/bash

rm -rf builddir32
export LDFLAGS="-lws2_32"
export CFLAGS="-g2 -O2"
LIBRARY_PATH="$HOME/lib32" \
PKG_CONFIG_PATH="$HOME/lib32/pkgconfig" \
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
-Dcombining=false \
-Ddebug=false \
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
-Dprefix=$HOME \
-Dpython=false \
-Dquickjs=false \
-Dreproducible=false \
-Druby=false \
-Dsm-scripting=false \
-Dsmb=false \
-Dspidermonkey=false \
-Dstatic=false \
-Dterminfo=false \
-Dtest=false \
-Dtre=false \
-Dtrue-color=false \
-Dutf-8=false \
-Dwithdebug=false \
-Dx=false \
-Dxbel=true \
-Dzlib=true \
-Dzstd=true || exit 4


/mingw32/bin/meson compile -C builddir32

# prepare zip
rm -rf $HOME/ELINKS32

mkdir -p $HOME/ELINKS32/src
mkdir -p $HOME/ELINKS32/po

install builddir32/src/elinks.exe $HOME/ELINKS32/src/elinks-lite.exe

cd builddir32/po
for i in *; do cp -v $i/LC_MESSAGES/elinks.mo $HOME/ELINKS32/po/$i.gmo; done
cd -

cd $HOME/ELINKS32/src
for i in $(ntldd.exe -R elinks-lite.exe | grep \\\\mingw32\\\\bin | cut -d'>' -f2 | cut -d' ' -f2); do cp -v $i . ; done
cd -
