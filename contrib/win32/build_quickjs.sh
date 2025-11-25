PREFIX=/opt/elinks
cd
rm -rf quickjs-2025-09-13
wget https://bellard.org/quickjs/quickjs-2025-09-13-2.tar.xz
tar xf quickjs-2025-09-13-2.tar.xz
cp Makefile.win32 quickjs-2025-09-13/
cd quickjs-2025-09-13
make -f Makefile.win32 PREFIX=$PREFIX CONFIG_WIN32=y
make -f Makefile.win32 install PREFIX=$PREFIX CONFIG_WIN32=y
