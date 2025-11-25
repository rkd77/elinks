PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error -DNDEBUG"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.8-src.tar.gz
rm -rf libhubbub-0.3.8
tar -xf libhubbub-0.3.8-src.tar.gz
make -C libhubbub-0.3.8 install -j1 Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
