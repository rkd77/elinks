#
# elinks docker development environment
#

# [*] base system

# get latest alpine
FROM alpine:edge

# prepare system
RUN apk update && apk add git meson gcc g++ cmake pkgconfig make patch wget coreutils
RUN cd /etc/apk && echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> repositories

# install
# [*] install libraries
RUN apk add \
 brotli-dev \
 brotli-static \
 bsd-compat-headers \
 bzip2-dev \
 bzip2-static \
 c-ares-dev \
 curl-dev \
 curl-static \
 expat-dev \
 expat-static \
 gettext-dev \
 gettext-static \
 libevent-dev \
 libevent-static \
 libidn2-dev \
 libidn2-static \
 libunistring-dev \
 libunistring-static \
 libwebp-dev \
 libwebp-static \
 libxml2-dev \
 libxml2-static \
 linux-headers \
 luajit-dev \
 make \
 musl-dev \
 ncurses-dev \
 ncurses-static \
 nghttp2-dev \
 nghttp2-static \
 nghttp3-dev \
 nghttp3-static \
 openssl-dev \
 openssl-libs-static \
 perl \
 quickjs-dev \
 sqlite-dev \
 sqlite-static \
 tre-dev \
 tre-static \
 upx \
 xz-dev \
 xz-static \
 zlib-dev \
 zlib-static \
 zstd-dev \
 zstd-static

## [*] netsurf libs
# get netsurf source
RUN cd /root; \
wget http://download.netsurf-browser.org/libs/releases/buildsystem-1.10.tar.gz; \
tar -xf buildsystem-1.10.tar.gz; \
make -C buildsystem-1.10 install PREFIX=/usr/local
##rm -rf buildsystem-1.10

#
RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libparserutils-0.2.5-src.tar.gz; \
tar -xf libparserutils-0.2.5-src.tar.gz; \
make -C libparserutils-0.2.5 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libparserutils-0.2.5

RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libwapcaplet-0.4.3-src.tar.gz; \
tar -xf libwapcaplet-0.4.3-src.tar.gz; \
make -C libwapcaplet-0.4.3 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libwapcaplet-0.4.3

RUN apk add gperf
RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.8-src.tar.gz; \
tar -xf libhubbub-0.3.8-src.tar.gz; \
make -C libhubbub-0.3.8 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libhubbub-0.3.8

RUN apk add libpsl-dev libpsl-static
RUN cd /root; \
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.2-src.tar.gz; tar -xf libcss-0.9.2-src.tar.gz ; \
export CFLAGS="-Wno-error"; \
make -C libcss-0.9.2 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libcss-0.9.2

RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.2-src.tar.gz; \
tar -xf libdom-0.4.2-src.tar.gz; \
make -C libdom-0.4.2 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libdom-0.4.2

## [*] elinks sources
# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

## [*] Compilation
#

ADD mes_static.sh /root/elinks/mes_static.sh
RUN cd /root/elinks; \
./mes_static.sh;

RUN apk add xxd
ADD mes_static_js.sh /root/elinks/mes_static_js.sh
RUN cd /root/elinks; \
./mes_static_js.sh;
