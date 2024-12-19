export LIBRARY_PATH="$HOME/64/lib"
export PKG_CONFIG_PATH="$HOME/64/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/64/include"
export CFLAGS="-O2 -I$HOME/64/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/64/include -Wno-error"
export LDFLAGS="-L$HOME/64/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.8-src.tar.gz
rm -rf libhubbub-0.3.8
tar -xf libhubbub-0.3.8-src.tar.gz
make -C libhubbub-0.3.8 install -j1 Q= PREFIX=$HOME/64 LIBDIR=lib COMPONENT_TYPE=lib-static
