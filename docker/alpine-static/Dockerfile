#
# [ x86_64 ] elinks docker development environment
#

# [*] base system

# get latest alpine
FROM alpine:edge

# prepare system
RUN apk update && apk add git meson gcc g++ cmake pkgconfig
RUN cd /etc/apk && echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> repositories

# install
# [*] install libraries
RUN cd /root && apk add \
 brotli-dev \
 brotli-static \
 bzip2-dev \
 bzip2-static \
 expat-dev \
 expat-static \
 libevent-dev \
 libevent-static \
 libidn-dev \
 libxml2-dev \
 linux-headers \
 luajit-dev \
 ncurses-dev \
 ncurses-static \
 openssl1.1-compat-dev \
 openssl1.1-compat-libs-static \
 quickjs-dev \
 sqlite-dev \
 sqlite-static \
 zlib-dev \
 zlib-static \
 zstd-dev \
 xz-dev \
 xz-static

# [*] libxml++5
RUN cd /root && apk add mm-common libtool autoconf automake make xz

# libxml++5 cd
RUN mkdir /root/tmp; cd /root/tmp; \
wget https://github.com/libxmlplusplus/libxmlplusplus/releases/download/5.0.2/libxml++-5.0.2.tar.xz; \
tar -xf libxml++-5.0.2.tar.xz;
ADD build_xmlplusplus.sh /root/tmp/libxml++-5.0.2/build_xmlplusplus.sh
RUN cd /root/tmp/libxml++-5.0.2; \
./build_xmlplusplus.sh

## [*] elinks sources
# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

## [*] Compilation
#
ADD mes_static.sh /root/elinks/mes_static.sh
RUN cd /root/elinks; \
./mes_static.sh;

ADD mes_static_js.sh /root/elinks/mes_static_js.sh
RUN cd /root/elinks; \
./mes_static_js.sh;
