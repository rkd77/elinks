PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libparserutils-0.2.5-src.tar.gz
rm -rf libparserutils-0.2.5
tar -xf libparserutils-0.2.5-src.tar.gz
make -C libparserutils-0.2.5 install -j1 Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
