cd
rm -rf quickjs-2024-01-13
wget https://bellard.org/quickjs/quickjs-2024-01-13.tar.xz
tar xf quickjs-2024-01-13.tar.xz
cd quickjs-2024-01-13
patch -p1 < ../quickjs-dos.diff
make -f Makefile.dos prefix=$HOME
make -f Makefile.dos install prefix=$HOME
