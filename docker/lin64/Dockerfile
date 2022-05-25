#
# [ lin64 ] elinks docker development environment v0.1d
#
# with quickjs support
#

# [*] base system

# get latest debian
FROM debian:latest

# prepare system - commond base for all platforms
RUN apt-get update && apt-get -y install bash \
  rsync vim screen git make automake

# [*] get sources and req libs
RUN cd /root && git clone https://github.com/bellard/quickjs && cd /root && git clone https://github.com/libxmlplusplus/libxmlplusplus && cd /root && apt-get install -y libxml2-dev g++

# [*] build quickjs
RUN cd /root/quickjs && make && make install

# [*] build libxmlplusplus
RUN apt-get install -y mm-common && cd /root/libxmlplusplus && ./autogen.sh && ./configure --enable-static --prefix=/usr/local && make && make install

RUN ln -s /usr/local/include/libxml++-5.0/libxml++ /usr/local/include/libxml++ && ln -s /usr/local/lib/libxml++-5.0/include/libxml++config.h /usr/local/include/libxml++config.h && ln -s /usr/include/libxml2/libxml /usr/include/libxml

# [*] prepare openssl library and javascript preqreqs
RUN apt-get -y install libssl-dev libsqlite3-dev zlib1g-dev liblzma-dev libtre-dev libidn11-dev libexpat1-dev

# disable caching for this step to get the newest elinks sources
ARG NOCACHE=1
RUN cd /root && git clone https://github.com/rkd77/elinks

