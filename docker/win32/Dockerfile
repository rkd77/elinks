#
# [ win32 ] elinks docker development environment v0.1a
#

# [*] base system

# get latest debian
FROM debian:latest

# prepare system
RUN apt-get update && apt-get -y install bash \
  rsync vim screen git make automake \
  gcc-mingw-w64-i686 g++-mingw-w64-i686

# [*] source build tools

# install sources build tools and update 
RUN apt-get install -y apt-src && \
  grep '^deb ' /etc/apt/sources.list | sed 's/deb /deb-src /' >> /etc/apt/sources.list && \
  apt-src update

# [*] install sources

# install sources for openssl and zlib1g-dev
RUN cd /root && apt-src install libssl-dev zlib1g-dev

# [*] elinks openssl development support

#
# build openssl library for win32
# should be win2k compatible
#
RUN cd /root && cd `ls -d /root/openssl-*` && \
sed -i '35 i \\n#undef AI_PASSIVE\n' crypto/bio/b_addr.c && \
CFLAGS="-DWINVER=0x0501 -D_WIN32_WINNT=0x0501" \
./Configure mingw \
  no-engine \
  no-dso \
  no-shared \
  no-asm \
  no-async \
  --prefix=/usr/local \
  --cross-compile-prefix=i686-w64-mingw32- && \
  make depend && \
  make && \
  make install_runtime_libs && \
  make install_dev

# [*} zlib sources

# build zlib library for win32
RUN cd /root && cd `ls -d /root/zlib-*` && \
CC="i686-w64-mingw32-gcc" \
LD="i686-w64-mingw32-ldd" \
./configure --static --prefix=/usr/local && \
make && \
make install

# [*] elinks sources

# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

