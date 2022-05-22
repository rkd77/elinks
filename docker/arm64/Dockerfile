#
# [ arm64 ] elinks docker development environment v0.1c
#

# [*] base system

# get latest debian
FROM debian:latest

# prepare system
RUN apt-get update && apt-get -y install bash \
  rsync vim screen git make automake

# [*] source build tools

# install sources build tools and update 
RUN apt-get install -y apt-src && \
  grep '^deb ' /etc/apt/sources.list | sed 's/deb /deb-src /' >> /etc/apt/sources.list && \
  apt-src update

# [*] install sources

# install sources for openssl and zlib1g-dev
RUN cd /root && apt-src install libssl-dev zlib1g-dev

# install dev tools [ platform dependent ] 

RUN apt-get -y install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

## [*] elinks openssl development support
# build openssl library for arm64
RUN cd /root && cd `ls -d /root/openssl-*` && \
./Configure linux-aarch64 \
  --prefix=/usr/local \
  --cross-compile-prefix=aarch64-linux-gnu- && \
  make depend && \
  make && \
  make install_runtime_libs && \
  make install_dev

## [*} zlib sources
# build zlib library for arm64
RUN cd /root && cd `ls -d /root/zlib-*` && \
CC="aarch64-linux-gnu-gcc" \
LD="aarch64-linux-gnu-ldd" \
./configure --static --prefix=/usr/local && \
make && \
make install

## [*] elinks sources
# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

