#
# [ djgpp ] elinks docker development environment v0.1c
#

# [*] base system

# get latest fedora
FROM fedora:42

# prepare system
RUN dnf -y update && dnf -y install bash wget \
  rsync vim screen git make automake meson unzip cmake patch upx gcc p7zip

RUN cd /root; wget http://www.mirrorservice.org/sites/ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-12.2.0-1ap.x86_64.rpm ; \
wget http://www.mirrorservice.org/sites/ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-c++-12.2.0-1ap.x86_64.rpm ; \
wget http://www.mirrorservice.org/sites/ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-tools-12.2.0-1ap.x86_64.rpm ; \
wget http://www.mirrorservice.org/sites/ftp.delorie.com/pub/djgpp/rpms/djcross-binutils-2.34-1ap.x86_64.rpm ; \
wget http://www.mirrorservice.org/sites/ftp.delorie.com/pub/djgpp/rpms/djcrx-2.05-5.x86_64.rpm ; \
rpm -Uhv *.rpm

# create user
## RUN useradd -m elinks -s /bin/bash

# djgpp libraries
RUN dnf -y install perl-FindBin pkgconf-pkg-config gettext
ADD unpack_zips.sh /root
RUN bash /root/unpack_zips.sh

# libidn2
ADD getprogname.diff /root/getprogname.diff
ADD build_libidn2.sh /root
RUN bash /root/build_libidn2.sh

# openssl
## ADD build_openssl.sh /home/elinks/
## RUN bash /home/elinks/build_openssl.sh

# brotli
ADD cc.py /root/cc.py
ADD brotli.diff /root/brotli.diff
ADD build_brotli.sh /root/
RUN bash /root/build_brotli.sh

# quickjs
RUN dnf -y install xz
ADD quickjs-dos.diff /root/
ADD build_quickjs.sh /root/
RUN bash /root/build_quickjs.sh

# netsurf_buildsystem
ADD build_netsurf.sh /root/
RUN bash /root/build_netsurf.sh

# parserutils
ADD build_parserutils.sh /root/
RUN bash /root/build_parserutils.sh

# libwapcaplet
ADD build_wapcaplet.sh /root/
RUN bash /root/build_wapcaplet.sh

# libhubbub
RUN dnf -y install gperf
ADD build_hubbub.sh /root/
RUN bash /root/build_hubbub.sh

# libcss
ADD build_libcss.sh /root/
RUN bash /root/build_libcss.sh

# libdom
ADD build_libdom.sh /root/
RUN bash /root/build_libdom.sh

# libcurl
##ADD build_curl.sh /root/
##RUN bash /root/build_curl.sh

# mujs
##ADD mujs.diff /root/
##ADD build_mujs.sh /root/
##RUN bash /root/build_mujs.sh

# dosemu
## RUN dnf install -y 'dnf-command(copr)'
## RUN dnf copr -y enable stsp/dosemu2
## RUN dnf install -y dosemu2

# get elinks source
RUN git clone https://github.com/rkd77/elinks /root/elinks

# additional perl packages
## RUN dnf install -y perl-autouse

# dosemu libs
## RUN echo /usr/lib64/fdpp > /etc/ld.so.conf.d/dosemu.conf
## RUN ldconfig

# nojs
ADD mes_djgpp.sh /root/elinks/
RUN bash /root/elinks/mes_djgpp.sh

# js
RUN dnf install -y xxd
ADD mes_djgpp_js.sh /root/elinks/mes_djgpp_js.sh
RUN bash /root/elinks/mes_djgpp_js.sh
