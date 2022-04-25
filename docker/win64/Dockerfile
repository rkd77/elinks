#
# [ win64 ] elinks docker development environment v0.1a
#

FROM debian:latest

RUN apt-get update; apt-get -y install rsync vim screen git make automake gcc-mingw-w64-x86-64 bash g++-mingw-w64-x86-64 libssl-dev

# [*] elinks openssl development support

# install sources build tools and update
RUN apt-get install -y apt-src && grep '^deb ' /etc/apt/sources.list | sed 's/deb /deb-src /' >> /etc/apt/sources.list && apt-src update

# install openssl library source code
RUN cd /root && apt-src install libssl-dev

# build openssl library for win64
RUN cd /root && cd `ls -d /root/openssl-*` && ./Configure mingw64 --cross-compile-prefix=x86_64-w64-mingw32- --prefix=/usr/local && make && make install

# [*] elinks sources

# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

