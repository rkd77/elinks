#
# [ djgpp ] elinks docker development environment v0.1c
#

# [*] base system

# get latest fedora
FROM fedora:37

# prepare system
RUN dnf -y update && dnf -y install bash wget \
  rsync vim screen git make automake meson unzip cmake patch

RUN cd /root; wget http://ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-12.2.0-1ap.x86_64.rpm ; \
wget http://ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-c++-12.2.0-1ap.x86_64.rpm ; \
wget http://ftp.delorie.com/pub/djgpp/rpms/djcross-gcc-12.2.0/djcross-gcc-tools-12.2.0-1ap.x86_64.rpm ; \
wget http://ftp.delorie.com/pub/djgpp/rpms/djcross-binutils-2.34-1ap.x86_64.rpm ; \
wget http://ftp.delorie.com/pub/djgpp/rpms/djcrx-2.05-5.x86_64.rpm ; \
rpm -Uhv *.rpm

# djgpp libraries

RUN cd /root; mkdir zip; cd zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/expat20br2.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/wat3211b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/xml2914b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/zlb1212b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/xz-525a.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/bz2-108a.zip; \
wget https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/apps/sqlite.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/licv116b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/lidn138b.zip;

RUN cd /root/zip; \
mkdir tmp; unzip expat20br2.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local; rm -rf tmp; \
mkdir tmp; unzip wat3211b.zip -d tmp; mv -f tmp/net/watt/inc tmp/net/watt/include; cp -a tmp/net/watt/include /usr/local/; cp -a tmp/net/watt/lib /usr/local; rm -rf tmp; \
mkdir tmp; unzip xml2914b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip zlb1212b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip xz-525a.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip bz2-108a.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip sqlite.zip -d tmp; mkdir tmp/tmp2; unzip tmp/SOURCE/SQLITE/SOURCES.ZIP -d tmp/tmp2; cp -a tmp/tmp2/examples/sqlite3.h /usr/local/include/; cp -a tmp/tmp2/examples/libsqlite3.a /usr/local/lib/; \
cp -a tmp/tmp2/sqlite3.pc /usr/local/lib/pkgconfig/; rm -rf tmp; \
mkdir tmp; unzip licv116b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip lidn138b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
sed -i -e 's|/dev/env/DJDIR|/usr/local|g' /usr/local/lib/pkgconfig/*.pc; \
sed -i -e 's|/dev/env/DJDIR|/usr/local|g' /usr/local/lib/*.la; \
sed -i -e 's/Libs\.private/#Libs.private/' /usr/local/lib/pkgconfig/sqlite3.pc

# openssl
RUN dnf -y install perl-FindBin; mkdir /root/tmp; cd /root/tmp; \
wget https://www.openssl.org/source/openssl-1.1.1s.tar.gz; \
tar -xf openssl-1.1.1s.tar.gz; \
cd openssl-1.1.1s; \
CFLAGS="-I/usr/local/include -DWATT32_NO_OLDIES -DSHUT_RD=0 -L/usr/local/lib -fcommon" \
./Configure no-threads \
  no-tests \
  -static \
  DJGPP \
  --prefix=/usr/local \
  --cross-compile-prefix=i586-pc-msdosdjgpp- && \
  make depend && \
  make -j12 && \
  make install_runtime_libs && \
  make install_dev ; \
rm -rf /root/tmp

# brotli
ADD cc.py /usr/local/bin/cc.py
ADD brotli.diff /root/brotli.diff
RUN mkdir /root/tmp; cd /root/tmp; \
wget https://github.com/google/brotli/archive/refs/tags/v1.0.9.tar.gz; \
tar -xf v1.0.9.tar.gz; \
cd brotli-1.0.9; \
patch -p1 < /root/brotli.diff; \
mkdir build; \
cd build; \
CC=cc.py cmake \
-DBUILD_SHARED_LIBS:BOOL=OFF \
-DBUILD_STATIC_LIBS:BOOL=ON \
-DCMAKE_AR=/usr/bin/i586-pc-msdosdjgpp-ar \
.. ; \
make -j12 VERBOSE=1; \
make install; \
rm -rf /root/tmp

# libxml++5
RUN dnf -y install mm-common libtool xz

# libxml++5 cd
RUN mkdir /root/tmp; cd /root/tmp; \
wget https://github.com/libxmlplusplus/libxmlplusplus/releases/download/5.0.2/libxml++-5.0.2.tar.xz; \
tar -xf libxml++-5.0.2.tar.xz;
ADD build_xmlplusplus.sh /root/tmp/libxml++-5.0.2/build_xmlplusplus.sh
RUN cd /root/tmp/libxml++-5.0.2; \
./build_xmlplusplus.sh

# quickjs
RUN dnf -y install xz; rm -rf /root/tmp; mkdir /root/tmp; cd /root/tmp; \
wget https://bellard.org/quickjs/quickjs-2021-03-27.tar.xz; \
tar xf quickjs-2021-03-27.tar.xz
ADD quickjs-dos.diff /root/tmp/quickjs-dos.diff
RUN cd /root/tmp/quickjs-2021-03-27; \
patch -p1 < ../quickjs-dos.diff; \
make -f Makefile.dos; \
mv -f /usr/local /usr/local2; \
make -f Makefile.dos; \
mv -f /usr/local2 /usr/local; \
make -f Makefile.dos install

# [*] elinks sources

# get elinks source
RUN dnf -y install pkgconf-pkg-config gettext
RUN cd /root; git clone  https://github.com/rkd77/elinks
ADD mes_djgpp.sh /root/elinks/mes_djgpp.sh
RUN cd /root/elinks; \
./mes_djgpp.sh;

ADD mes_djgpp_js.sh /root/elinks/mes_djgpp_js.sh
RUN cd /root/elinks; \
./mes_djgpp_js.sh;

