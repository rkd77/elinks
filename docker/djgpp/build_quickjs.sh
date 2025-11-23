PREFIX=/opt/elinks
cp quickjs-dos.diff ~/
cd
rm -rf quickjs-2025-09-13
wget https://bellard.org/quickjs/quickjs-2025-09-13-2.tar.xz
tar xf quickjs-2025-09-13-2.tar.xz
cd quickjs-2025-09-13
patch -p1 < ../quickjs-dos.diff
CFLAGS=-DNDEBUG make PREFIX=$PREFIX
make install PREFIX=$PREFIX
