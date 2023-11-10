cd
wget https://github.com/libexpat/libexpat/releases/download/R_2_5_0/expat-2.5.0.tar.gz
rm -rf expat-2.5.0
tar -xf expat-2.5.0.tar.gz
cd expat-2.5.0
./configure --host=i686-w64-mingw32 --prefix=$HOME \
	--enable-static=yes \
	--enable-shared=no \
	CC=i686-w64-mingw32-gcc \
	CPPFLAGS="-I$HOME/include -Wall" \
	LDFLAGS="-L$HOME/lib"
make -j $(nproc)
make install
