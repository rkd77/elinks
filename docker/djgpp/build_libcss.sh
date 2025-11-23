PREFIX=/opt/elinks
cd
rm -rf libcss-0.9.2
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.2-src.tar.gz; tar -xf libcss-0.9.2-src.tar.gz
tar xf libcss-0.9.2-src.tar.gz
cd libcss-0.9.2
#patch -p1 < ../libCSS-restrict.diff || exit 1
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile
cd ..
export CFLAGS="-O2 -Wno-error"
export CXXFLAGS="-O2 -Wno-error"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
export HOST="i586-pc-msdosdjgpp"
export BUILD_CC=gcc
make -C libcss-0.9.2 -j1 Q= PREFIX=$PREFIX LIBDIR=lib gen

export LIBRARY_PATH="$PREFIX/lib"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export C_INCLUDE_PATH="$PREFIX/include"
export CFLAGS="-O2 -I$PREFIX/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export CXXFLAGS="-O2 -I$PREFIX/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"
export LDFLAGS="-L$PREFIX/lib"
export CC="i586-pc-msdosdjgpp-gcc"
export AR="i586-pc-msdosdjgpp-ar"
export HOST="i586-pc-msdosdjgpp"
make -C libcss-0.9.2 install -j$(nproc) Q= PREFIX=$PREFIX LIBDIR=lib COMPONENT_TYPE=lib-static
