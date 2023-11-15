cd
wget https://curl.se/download/curl-8.4.0.tar.xz
rm -rf curl-8.4.0
tar -xf curl-8.4.0.tar.xz
cd curl-8.4.0
PKG_CONFIG_PATH="$HOME/lib/pkgconfig" CPPFLAGS="-I$HOME/include" LIBS="-L$HOME/lib -liconv -L$HOME/lib -lbrotlicommon -L$HOME/lib -lunistring -L$HOME/lib -liconv" \
./configure --host=i586-pc-msdosdjgpp --with-ssl --with-zlib --with-brotli --with-libidn2 --disable-threaded-resolver --with-srp --prefix=$HOME
make -j $(nproc)
make install
