PREFIX=/opt/elinks
cp brotli.diff ~/
cp cc.py ~/
cd
wget https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz
rm -rf brotli-1.1.0
tar -xf v1.1.0.tar.gz
cd brotli-1.1.0
patch -p1 < ../brotli.diff || exit 1
mkdir build
cd build
CC=$HOME/cc.py cmake \
-DCMAKE_INSTALL_PREFIX=$PREFIX \
-DBUILD_SHARED_LIBS:BOOL=OFF \
-DBUILD_STATIC_LIBS:BOOL=ON \
-DCMAKE_AR=/usr/bin/i586-pc-msdosdjgpp-ar \
..
make -j $(nproc) VERBOSE=1
make install
