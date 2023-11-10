export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error"
export LDFLAGS="-L$HOME/lib"
export CC="i686-w64-mingw32-gcc"
export AR="i686-w64-mingw32-ar"
cd
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.7-src.tar.gz
rm -rf libhubbub-0.3.7
tar -xf libhubbub-0.3.7-src.tar.gz
make -C libhubbub-0.3.7 install -j1 Q= PREFIX=$HOME LIBDIR=lib COMPONENT_TYPE=lib-static
