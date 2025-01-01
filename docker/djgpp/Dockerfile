#
# [ djgpp ] elinks docker development environment v0.1c
#

# [*] base system

# get latest fedora
FROM fedora:40

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
RUN useradd -m elinks -s /bin/bash

# djgpp libraries
RUN dnf -y install perl-FindBin pkgconf-pkg-config gettext
ADD unpack_zips.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/unpack_zips.sh

# libidn2
ADD getprogname.diff /home/elinks/getprogname.diff
ADD build_libidn2.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_libidn2.sh

# openssl
##ADD build_openssl.sh /home/elinks/
##RUN runuser -u elinks bash /home/elinks/build_openssl.sh

# brotli
ADD cc.py /usr/local/bin/cc.py
ADD brotli.diff /home/elinks/brotli.diff
ADD build_brotli.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_brotli.sh

# quickjs
RUN dnf -y install xz
ADD quickjs-dos.diff /home/elinks/
ADD build_quickjs.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_quickjs.sh

# netsurf_buildsystem
ADD build_netsurf.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_netsurf.sh

# parserutils
ADD build_parserutils.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_parserutils.sh

# libwapcaplet
ADD build_wapcaplet.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_wapcaplet.sh

# libhubbub
RUN dnf -y install gperf
ADD build_hubbub.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_hubbub.sh

# libcss
ADD build_libcss.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_libcss.sh

# libdom
ADD build_libdom.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_libdom.sh

# libcurl
##ADD build_curl.sh /home/elinks/
##RUN runuser -u elinks bash /home/elinks/build_curl.sh

# mujs
ADD mujs.diff /home/elinks/
ADD build_mujs.sh /home/elinks/
RUN runuser -u elinks bash /home/elinks/build_mujs.sh

# dosemu
## RUN dnf install -y 'dnf-command(copr)'
## RUN dnf copr -y enable stsp/dosemu2
## RUN dnf install -y dosemu2

# get elinks source
RUN runuser -u elinks git clone https://github.com/rkd77/elinks /home/elinks/elinks

# additional perl packages
## RUN dnf install -y perl-autouse

# dosemu libs
## RUN echo /usr/lib64/fdpp > /etc/ld.so.conf.d/dosemu.conf
## RUN ldconfig

# nojs
ADD mes_djgpp.sh /home/elinks/elinks/
RUN runuser -u elinks bash /home/elinks/elinks/mes_djgpp.sh

# js
RUN dnf install -y xxd
ADD mes_djgpp_js.sh /home/elinks/elinks/mes_djgpp_js.sh
RUN runuser -u elinks bash /home/elinks/elinks/mes_djgpp_js.sh
