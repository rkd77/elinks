#!/bin/bash

# script for Windows64 build using msys2

rm -rf builddir3
export LDFLAGS="-lws2_32"
export CFLAGS="-g2 -O2"
LIBRARY_PATH="$HOME/64U/lib" \
PKG_CONFIG_PATH="$HOME/64U/lib/pkgconfig" \
/ucrt64/bin/meson setup builddir3 \
-D88-colors=false \
-D256-colors=false \
-Dapidoc=false \
-Dbacktrace=false \
-Dbittorrent=false \
-Dbrotli=true \
-Dbzlib=false \
-Dcgi=false \
-Dcodepoint=false \
-Dcss=true \
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
-Dprefix=$HOME/64U \
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
-Dxbel=false \
-Dzlib=true \
-Dzstd=true || exit 1

/ucrt64/bin/meson compile -C builddir3 || exit 2

# prepare zip
mkdir -p $HOME/ELINKS64/src
mkdir -p $HOME/ELINKS64/po

install builddir3/src/elinks.exe $HOME/ELINKS64/src/

cd builddir3/po
for i in *; do cp -v $i/LC_MESSAGES/elinks.mo $HOME/ELINKS64/po/$i.gmo; done
cd -

cd $HOME/ELINKS64/src
for i in $(ldd elinks.exe | grep /ucrt64/bin | cut -d' ' -f3); do cp -v $i . ; done
cd -
