#
# [ djgpp ] elinks docker development environment v0.1c
#

# [*] base system

# get latest fedora
FROM fedora:38

# prepare system
RUN dnf -y update && dnf -y install bash wget \
  rsync vim screen git make automake meson unzip cmake patch upx gcc

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
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/zlb1213b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/xz-525a.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/bz2-108a.zip; \
wget https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/apps/sqlite.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/licv116b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/lus0910b.zip; \
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/lua522b.zip

RUN cd /root/zip; \
mkdir tmp; unzip expat20br2.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local; rm -rf tmp; \
mkdir tmp; unzip wat3211b.zip -d tmp; mv -f tmp/net/watt/inc tmp/net/watt/include; cp -a tmp/net/watt/include /usr/local/; cp -a tmp/net/watt/lib /usr/local; rm -rf tmp; \
mkdir tmp; unzip zlb1213b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip xz-525a.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip bz2-108a.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip sqlite.zip -d tmp; mkdir tmp/tmp2; unzip tmp/SOURCE/SQLITE/SOURCES.ZIP -d tmp/tmp2; cp -a tmp/tmp2/examples/sqlite3.h /usr/local/include/; cp -a tmp/tmp2/examples/libsqlite3.a /usr/local/lib/; \
cp -a tmp/tmp2/sqlite3.pc /usr/local/lib/pkgconfig/; rm -rf tmp; \
mkdir tmp; unzip licv116b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
mkdir tmp; unzip lus0910b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
sed -i -e 's|/dev/env/DJDIR|/usr/local|g' /usr/local/lib/pkgconfig/*.pc; \
sed -i -e 's|/dev/env/DJDIR|/usr/local|g' /usr/local/lib/*.la; \
sed -i -e 's/Libs\.private/#Libs.private/' /usr/local/lib/pkgconfig/sqlite3.pc; \
mkdir tmp; unzip lua522b.zip -d tmp; cp -a tmp/include /usr/local/; cp -a tmp/lib /usr/local/; rm -rf tmp; \
cat <<EOF > /usr/local/lib/pkgconfig/lua.pc
prefix=/usr/local
exec_prefix=\$\{prefix\}
libdir=\$\{exec_prefix\}/lib
includedir=\$\{prefix\}/include

Name: Lua
Description: An Extensible Extension Language
Version: 5.2
Requires:
Libs: -llua -lm
Cflags: -I\$\{includedir\}
EOF

# openssl
RUN dnf -y install perl-FindBin; mkdir /root/tmp; cd /root/tmp; \
wget https://www.openssl.org/source/openssl-1.1.1u.tar.gz; \
tar -xf openssl-1.1.1u.tar.gz; \
cd openssl-1.1.1u; \
CFLAGS="-I/usr/local/include -DWATT32_NO_OLDIES -DSHUT_RD=0 -L/usr/local/lib -fcommon" \
./Configure no-threads \
  no-tests \
  -static \
  DJGPP \
  --prefix=/usr/local \
  --cross-compile-prefix=i586-pc-msdosdjgpp- && \
  make depend && \
  make -j`nproc` && \
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
make -j`nproc` VERBOSE=1; \
make install; \
rm -rf /root/tmp

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

# libidn2
RUN rm -rf /root/tmp; mkdir /root/tmp; cd /root/tmp; \
wget https://ftp.gnu.org/gnu/libidn/libidn2-2.3.4.tar.gz; \
tar xvf libidn2-2.3.4.tar.gz
ADD getprogname.diff /root/tmp/getprogname.diff
ADD build_libidn2.sh /root/tmp/libidn2-2.3.4
RUN cd /root/tmp/libidn2-2.3.4; \
patch -p1 < ../getprogname.diff; \
./build_libidn2.sh

RUN dnf -y install pkgconf-pkg-config gettext

# netsurf
RUN cd /root; \
wget http://download.netsurf-browser.org/libs/releases/buildsystem-1.9.tar.gz; \
tar -xf buildsystem-1.9.tar.gz; \
make -C buildsystem-1.9 install PREFIX=/usr/local
##rm -rf buildsystem-1.9

#
RUN cd /root; \
export LIBRARY_PATH="/usr/local/lib"; \
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"; \
export C_INCLUDE_PATH="/usr/local/include"; \
export CFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export CXXFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export LDFLAGS="-L/usr/local/lib"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
wget http://download.netsurf-browser.org/libs/releases/libparserutils-0.2.4-src.tar.gz; \
tar -xf libparserutils-0.2.4-src.tar.gz; \
make -C libparserutils-0.2.4 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libparserutils-0.2.4

RUN cd /root; \
export LIBRARY_PATH="/usr/local/lib"; \
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"; \
export C_INCLUDE_PATH="/usr/local/include"; \
export CFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export CXXFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export LDFLAGS="-L/usr/local/lib"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
wget http://download.netsurf-browser.org/libs/releases/libwapcaplet-0.4.3-src.tar.gz; \
tar -xf libwapcaplet-0.4.3-src.tar.gz; \
make -C libwapcaplet-0.4.3 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libwapcaplet-0.4.3

RUN cd /root; \
export LIBRARY_PATH="/usr/local/lib"; \
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"; \
export C_INCLUDE_PATH="/usr/local/include"; \
export CFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export CXXFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export LDFLAGS="-L/usr/local/lib"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
wget http://download.netsurf-browser.org/libs/releases/libhubbub-0.3.7-src.tar.gz; \
tar -xf libhubbub-0.3.7-src.tar.gz; \
make -C libhubbub-0.3.7 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libhubbub-0.3.7

RUN cd /root; \
rm -rf libcss-0.9.1; \
wget http://download.netsurf-browser.org/libs/releases/libcss-0.9.1-src.tar.gz; tar -xf libcss-0.9.1-src.tar.gz
ADD libCSS-restrict.diff /root/libCSS-restrict.diff
RUN cd /root/libcss-0.9.1; patch -p1 < /root/libCSS-restrict.diff; \
printf '\ngen: $(PRE_TARGETS)\n' >> Makefile; \
cd ..; \
export LIBRARY_PATH="/usr/local/lib"; \
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"; \
export C_INCLUDE_PATH="/usr/local/include"; \
export CFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export CXXFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export LDFLAGS="-L/usr/local/lib"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
export HOST="i586-pc-msdosdjgpp"; \
export BUILD_CC=cc; \
mv -f /usr/local /usr/local3; \
mkdir -p /usr/local/share; \
cp -a /usr/local3/share/netsurf-buildsystem /usr/local/share/; \
make -C libcss-0.9.1 -j1 Q= PREFIX=/usr/local LIBDIR=lib gen
RUN cd /root; \
mv -f /usr/local /usr/local4; \
mv -f /usr/local3 /usr/local; \
export LIBRARY_PATH="/usr/local/lib"; \
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"; \
export C_INCLUDE_PATH="/usr/local/include"; \
export CFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export CXXFLAGS="-O2 -I/usr/local/include -Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__"; \
export LDFLAGS="-L/usr/local/lib"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
export HOST="i586-pc-msdosdjgpp"; \
make -C libcss-0.9.1 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libcss-0.9.1

RUN cd /root; \
export CFLAGS="-Wno-error"; \
export CC="i586-pc-msdosdjgpp-gcc"; \
export AR="i586-pc-msdosdjgpp-ar"; \
wget http://download.netsurf-browser.org/libs/releases/libdom-0.4.1-src.tar.gz; \
tar -xf libdom-0.4.1-src.tar.gz; \
make -C libdom-0.4.1 install -j1 Q= PREFIX=/usr/local LIBDIR=lib COMPONENT_TYPE=lib-static
##rm -rf libdom-0.4.1

# [*] elinks sources

# get elinks source
RUN cd /root; git clone  https://github.com/rkd77/elinks
ADD mes_djgpp.sh /root/elinks/mes_djgpp.sh
RUN cd /root/elinks; \
./mes_djgpp.sh;

# ADD mes_djgpp_js.sh /root/elinks/mes_djgpp_js.sh
# RUN cd /root/elinks; \
# ./mes_djgpp_js.sh;
