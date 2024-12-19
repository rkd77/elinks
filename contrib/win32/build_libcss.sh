cd
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.2-src.tar.gz; tar -xf libcss-0.9.2-src.tar.gz
rm -rf libcss-0.9.2
tar -xf libcss-0.9.2-src.tar.gz
cd libcss-0.9.2
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile
cd ..
export LIBRARY_PATH="$HOME/64/lib"
export PKG_CONFIG_PATH="$HOME/64/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/64/include"
export CFLAGS="-O2 -I$HOME/64/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/64/include -Wno-error"
export LDFLAGS="-L$HOME/64/lib"
export BUILD_CC=cc
make -C libcss-0.9.2 -j12 Q= PREFIX=$HOME/64 LIBDIR=lib gen

export LIBRARY_PATH="$HOME/64/lib"
export PKG_CONFIG_PATH="$HOME/64/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/64/include"
export CFLAGS="-O2 -I$HOME/64/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/64/include -Wno-error"
export LDFLAGS="-L$HOME/64/lib"
make -C libcss-0.9.2 install -j12 Q= PREFIX=$HOME/64 LIBDIR=lib COMPONENT_TYPE=lib-static
