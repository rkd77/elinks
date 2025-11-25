PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libwapcaplet-0.4.3-src.tar.gz
rm -rf libwapcaplet-0.4.3
tar -xf libwapcaplet-0.4.3-src.tar.gz
make -C libwapcaplet-0.4.3 install -j1 Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
