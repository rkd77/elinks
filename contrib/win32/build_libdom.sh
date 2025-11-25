PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.2-src.tar.gz
rm -rf libdom-0.4.2
tar -xf libdom-0.4.2-src.tar.gz
make -C libdom-0.4.2 install -j12 Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
