PREFIX=/opt/elinks
cp getprogname.diff ~/
cd
rm -rf libidn2-2.3.8
wget https://ftp.gnu.org/gnu/libidn/libidn2-2.3.8.tar.gz
tar xvf libidn2-2.3.8.tar.gz
cd libidn2-2.3.8
patch -p1 < ../getprogname.diff || exit 1

export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig

./configure \
--prefix=$PREFIX \
--host=i586-pc-msdosdjgpp \
--enable-static=yes \
--enable-shared=no \
--disable-documentation

make -j $(nproc)
make install
