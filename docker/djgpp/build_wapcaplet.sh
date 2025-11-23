PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export LDFLAGS="-L$PREFIX/lib"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
cd
wget http://download.netsurf-browser.org/libs/releases/libwapcaplet-0.4.3-src.tar.gz
rm -rf libwapcaplet-0.4.3
tar -xf libwapcaplet-0.4.3-src.tar.gz
make -C libwapcaplet-0.4.3 install -j$(nproc) Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
