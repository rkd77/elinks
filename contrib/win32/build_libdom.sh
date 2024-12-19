export LIBRARY_PATH="$HOME/64/lib"
export PKG_CONFIG_PATH="$HOME/64/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/64/include"
export CFLAGS="-O2 -I$HOME/64/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/64/include -Wno-error"
export LDFLAGS="-L$HOME/64/lib"
cd
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.2-src.tar.gz
rm -rf libdom-0.4.2
tar -xf libdom-0.4.2-src.tar.gz
make -C libdom-0.4.2 install -j12 Q= PREFIX=$HOME/64 LIBDIR=lib COMPONENT_TYPE=lib-static
