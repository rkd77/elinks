cd
wget https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz
rm -rf libiconv-1.17
tar xf libiconv-1.17.tar.gz
cd libiconv-1.17
./configure --host=x86_64-w64-mingw32 --prefix=$HOME \
        --enable-static=yes \
        --enable-shared=no \
        CC=x86_64-w64-mingw32-gcc \
        CPPFLAGS="-I$HOME/include -Wall" \
        LDFLAGS="-L$HOME/lib"
make -j $(nproc)
make install
