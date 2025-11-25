cd
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.2-src.tar.gz; tar -xf libcss-0.9.2-src.tar.gz
rm -rf libcss-0.9.2
tar -xf libcss-0.9.2-src.tar.gz
cd libcss-0.9.2
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile
cd ..
PREFIX=/opt/elinks
export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
export BUILD_CC=cc
make -C libcss-0.9.2 -j12 Q= PREFIX=$PREFIX LIBDIR=lib gen

export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error"
export LDFLAGS="-L$PREFIX/lib"
make -C libcss-0.9.2 install -j12 Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
