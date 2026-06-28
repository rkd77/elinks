#!/bin/sh

PREFIX=/opt/elinks
cp 1348.diff ~/
cd
rm -rf quickjs-0.15.1
wget https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.15.1.tar.gz
tar xf v0.15.1.tar.gz
cd quickjs-0.15.1
patch -p1 < ../1348.diff

mkdir -p cross
cp -a ~/linux-djgpp.txt cross/

rm -rf /tmp/builddir_qjs

LIBRARY_PATH="$PREFIX/lib" \
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" \
C_INCLUDE_PATH="$PREFIX/include" \
CFLAGS="-O2 -I$PREFIX/include -DNDEBUG -Wno-error -fpermissive" \
CXXFLAGS="-O2 -I$PREFIX/include" \
LDFLAGS="-L$PREFIX/lib" \
meson setup /tmp/builddir_qjs --cross-file cross/linux-djgpp.txt \
-Dprefix=$PREFIX \
-Dexamples=disabled \
-Dlibc=false \
-Dtests=disabled \
-Dtools=disabled \
-Ddefault_library=static || exit 1

meson compile -C /tmp/builddir_qjs 'codegen_builtin-array-fromasync.h' || exit 2
meson compile -C /tmp/builddir_qjs 'libunicode-table.h' || exit 4
meson compile -C /tmp/builddir_qjs || exit 5

meson install -C /tmp/builddir_qjs || exit 6
