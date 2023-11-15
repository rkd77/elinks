cd
rm -rf libcss-0.9.1
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.1-src.tar.gz; tar -xf libcss-0.9.1-src.tar.gz
tar xf libcss-0.9.1-src.tar.gz
cd libcss-0.9.1
patch -p1 < ../libCSS-restrict.diff || exit 1
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile
cd ..
export CFLAGS="-O2 -Wno-error"
export CXXFLAGS="-O2 -Wno-error"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
export HOST="i586-pc-msdosdjgpp"
export BUILD_CC=gcc
make -C libcss-0.9.1 -j1 Q= PREFIX=$HOME LIBDIR=lib gen

export LIBRARY_PATH="$HOME/lib"
export PKG_CONFIG_PATH="$HOME/lib/pkgconfig"
export C_INCLUDE_PATH="$HOME/include"
export CFLAGS="-O2 -I$HOME/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export CXXFLAGS="-O2 -I$HOME/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export LDFLAGS="-L$HOME/lib"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
export HOST="i586-pc-msdosdjgpp"
make -C libcss-0.9.1 install -j1 Q= PREFIX=$HOME LIBDIR=lib COMPONENT_TYPE=lib-static
