#
# elinks docker development environment
#

# [*] base system

# get latest alpine
FROM alpine:edge

# prepare system
RUN apk update && apk add git meson gcc g++ cmake pkgconfig make patch wget
RUN cd /etc/apk && echo https://dl-cdn.alpinelinux.org/alpine/edge/testing >> repositories

# install
# [*] install libraries
RUN apk add \
 brotli-dev \
 brotli-static \
 bzip2-dev \
 bzip2-static \
 curl-dev \
 curl-static \
 expat-dev \
 expat-static \
 libevent-dev \
 libevent-static \
 libidn2-dev \
 libidn2-static \
 libxml2-dev \
 libxml2-static \
 linux-headers \
 luajit-dev \
 ncurses-dev \
 ncurses-static \
 openssl-dev \
 openssl-libs-static \
 perl \
 quickjs-dev \
 sqlite-dev \
 sqlite-static \
 tre-dev \
 tre-static \
 libunistring-dev \
 libunistring-static \
 zlib-dev \
 zlib-static \
 zstd-dev \
 zstd-static \
 xz-dev \
 xz-static \
 make \
 nghttp2-dev \
 nghttp2-static \
 c-ares-dev \
 c-ares-static

## [*] netsurf libs
# get netsurf source
RUN cd /root; \
wget http://download.netsurf-browser.org/libs/releases/buildsystem-1.9.tar.gz; \
tar -xf buildsystem-1.9.tar.gz; \
make -C buildsystem-1.9 install PREFIX=/usr/local
##rm -rf buildsystem-1.9

#
RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libparserutils-0.2.4-src.tar.gz; \
tar -xf libparserutils-0.2.4-src.tar.gz; \
make -C libparserutils-0.2.4 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libparserutils-0.2.4

RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libwapcaplet-0.4.3-src.tar.gz; \
tar -xf libwapcaplet-0.4.3-src.tar.gz; \
make -C libwapcaplet-0.4.3 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libwapcaplet-0.4.3

RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.7-src.tar.gz; \
tar -xf libhubbub-0.3.7-src.tar.gz; \
make -C libhubbub-0.3.7 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libhubbub-0.3.7

RUN cd /root; \
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.1-src.tar.gz; tar -xf libcss-0.9.1-src.tar.gz
ADD libCSS-restrict.diff /root/libCSS-restrict.diff
RUN cd /root/libcss-0.9.1; patch -p1 < /root/libCSS-restrict.diff; cd .. ;\
export CFLAGS="-Wno-error"; \
make -C libcss-0.9.1 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libcss-0.9.1

RUN cd /root; \
export CFLAGS="-Wno-error"; \
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.1-src.tar.gz; \
tar -xf libdom-0.4.1-src.tar.gz; \
make -C libdom-0.4.1 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libdom-0.4.1

## [*] elinks sources
# get elinks source
RUN cd /root; git clone https://github.com/rkd77/elinks

## [*] Compilation
#
RUN apk add upx bsd-compat-headers

ADD mes_static.sh /root/elinks/mes_static.sh
RUN cd /root/elinks; \
./mes_static.sh;

ADD mes_static_js.sh /root/elinks/mes_static_js.sh
RUN cd /root/elinks; \
./mes_static_js.sh;
