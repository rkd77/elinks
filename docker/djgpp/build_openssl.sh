PREFIX=/opt/elinks
cd
wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz
rm -rf openssl-1.1.1w
tar -xf openssl-1.1.1w.tar.gz
cd openssl-1.1.1w
CFLAGS="-I$PREFIX/include -DWATT32_NO_OLDIES -DSHUT_RD=0 -L$PREFIX/lib -fcommon" \
./Configure no-threads \
  no-tests \
  -static \
  DJGPP \
  --prefix=$PREFIX \
  --cross-compile-prefix=i586-pc-msdosdjgpp- && \
  make depend && \
  make build_libs -j $(nproc) && \
  make install_runtime_libs && \
  make install_dev
