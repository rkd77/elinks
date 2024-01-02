#
# [ win64 ] elinks docker development environment v0.1c
#

# [*] base system

# get latest debian
FROM debian:bookworm

# prepare system
RUN apt-get update && apt-get -y install bash \
  rsync vim screen git make automake grep sed meson gperf

# [*] source build tools

# install sources build tools and update 
RUN apt-get install -y apt-src && \
  sed -i -e 's/Types: deb/Types: deb deb-src/g' /etc/apt/sources.list.d/*sources && \
  apt-src update

# install dev tools [ platform dependent ] 
RUN apt-get install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64

# install wine
## RUN apt-get install -y wine

# create user
RUN useradd -m elinks -s /bin/bash

# wget
RUN apt-get install -y wget cmake pkg-config

# build expat
ADD build_expat.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_expat.sh

# netsurf buildsystem
ADD build_netsurf.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_netsurf.sh

# build iconv
ADD build_iconv.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_iconv.sh

# parserutils
ADD build_parserutils.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_parserutils.sh

# wapcaplet
ADD build_wapcaplet.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_wapcaplet.sh

# hubbub
ADD build_hubbub.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_hubbub.sh

# patch
RUN apt-get install -y patch gcc

# libcss
ADD build_libcss.sh /home/elinks/
ADD libCSS-restrict.diff /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_libcss.sh

# libdom
ADD build_libdom.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_libdom.sh

# lua
ADD build_lua.sh /home/elinks/
ADD lua.diff /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_lua.sh

# sqlite3
ADD build_sqlite3.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_sqlite3.sh

# mujs
ADD build_mujs.sh /home/elinks/
ADD mujs.diff /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/build_mujs.sh

# curl
RUN apt-get install -y unzip
ADD unpack_curl.sh /home/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/unpack_curl.sh

# elinks sources
RUN apt-get install -y git
RUN runuser -u elinks git clone https://github.com/rkd77/elinks /home/elinks/elinks

# build without js
ADD mes.sh /home/elinks/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/elinks/mes.sh

# build with js
ADD mes_js.sh /home/elinks/elinks/
RUN runuser -u elinks /bin/bash /home/elinks/elinks/mes_js.sh
