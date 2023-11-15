cd
wget https://www.openssl.org/source/openssl-1.1.1w.tar.gz
rm -rf openssl-1.1.1w
tar -xf openssl-1.1.1w.tar.gz
cd openssl-1.1.1w
CFLAGS="-I$HOME/include -DWATT32_NO_OLDIES -DSHUT_RD=0 -L$HOME/lib -fcommon" \
./Configure no-threads \
  no-tests \
  -static \
  DJGPP \
  --prefix=$HOME \
  --cross-compile-prefix=i586-pc-msdosdjgpp- && \
  make depend && \
  make build_libs -j $(nproc) && \
  make install_runtime_libs && \
  make install_dev
