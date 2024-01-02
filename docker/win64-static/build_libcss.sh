cd
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.2-src.tar.gz; tar -xf libcss-0.9.2-src.tar.gz
rm -rf libcss-0.9.2
tar -xf libcss-0.9.2-src.tar.gz
cd libcss-0.9.2
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile
cd ..
export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error"
export LDFLAGS="-L$HOME/lib"
export CC="x86_64-w64-mingw32-gcc"
export AR="x86_64-w64-mingw32-ar"
export HOST="x86_64-w64-mingw32"
export BUILD_CC=cc
make -C libcss-0.9.2 -j1 Q= PREFIX=$HOME LIBDIR=lib gen

export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error"
export LDFLAGS="-L$HOME/lib"
export CC="x86_64-w64-mingw32-gcc"
export AR="x86_64-w64-mingw32-ar"
export HOST="x86_64-w64-mingw32"
make -C libcss-0.9.2 install -j1 Q= PREFIX=$HOME LIBDIR=lib COMPONENT_TYPE=lib-static
