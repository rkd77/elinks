export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error"
export LDFLAGS="-L$HOME/lib"
export CC="x86_64-w64-mingw32-gcc"
export AR="x86_64-w64-mingw32-ar"
cd
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.2-src.tar.gz
rm -rf libdom-0.4.2
tar -xf libdom-0.4.2-src.tar.gz
make -C libdom-0.4.2 install -j1 Q= PREFIX=$HOME LIBDIR=lib COMPONENT_TYPE=lib-static
