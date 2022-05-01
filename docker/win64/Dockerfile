#
# [ win64 ] elinks docker development environment v0.1b
#

# [*] base system

# get latest debian
FROM debian:latest

# prepare system
RUN apt-get update && apt-get -y install bash \
  rsync vim screen git make automake \
  gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

# [*] source build tools

# install sources build tools and update 
RUN apt-get install -y apt-src && \
  grep '^deb ' /etc/apt/sources.list | sed 's/deb /deb-src /' >> /etc/apt/sources.list && \
  apt-src update

# [*] install sources

# install sources for openssl and zlib1g-dev
RUN cd /root && apt-src install libssl-dev zlib1g-dev

# [*] elinks openssl development support

# build openssl library for win64
RUN cd /root && cd `ls -d /root/openssl-*` && \
./Configure mingw64 \
  --prefix=/usr/local \
  --cross-compile-prefix=x86_64-w64-mingw32- && \
  make depend && \
  make && \
  make install_runtime_libs && \
  make install_dev

# [*} zlib sources

# build zlib library for win64
RUN cd /root && cd `ls -d /root/zlib-*` && \
CC="x86_64-w64-mingw32-gcc" \
LD="x86_64-w64-mingw32-ldd" \
./configure --static --prefix=/usr/local && \
make && \
make install

# [*] elinks sources

# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

