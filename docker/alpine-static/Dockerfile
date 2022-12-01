#
# [ x86_64 ] elinks docker development environment
#

# [*] base system

# get latest alpine
FROM alpine:edge

# prepare system
RUN apk update && apk add git meson gcc g++ cmake pkgconfig

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
 sqlite-dev \
 sqlite-static \
 zlib-dev \
 zlib-static \
 zstd-dev \
 xz-dev \
 xz-static

## [*] elinks sources
# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

## [*] Compilation
#
ADD mes_static.sh /root/elinks/mes_static.sh
RUN cd /root/elinks; \
./mes_static.sh;
