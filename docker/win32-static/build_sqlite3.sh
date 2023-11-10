cd
wget https://www.sqlite.org/2023/sqlite-autoconf-3440000.tar.gz
rm -rf sqlite-autoconf-3440000
tar xf sqlite-autoconf-3440000.tar.gz
cd sqlite-autoconf-3440000
./configure --host=i686-w64-mingw32 --prefix=$HOME \
        --enable-static=yes \
        --enable-shared=no \
        --disable-readline \
        CC=i686-w64-mingw32-gcc \
        CPPFLAGS="-I$HOME/include -Wall" \
        LDFLAGS="-L$HOME/lib"
make -j $(nproc)
make install
