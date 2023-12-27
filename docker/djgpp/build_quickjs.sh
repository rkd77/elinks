cd
rm -rf quickjs-2023-12-09
wget https://bellard.org/quickjs/quickjs-2023-12-09.tar.xz
tar xf quickjs-2023-12-09.tar.xz
cd quickjs-2023-12-09
patch -p1 < ../quickjs-dos.diff
make -f Makefile.dos prefix=$HOME
make -f Makefile.dos install prefix=$HOME
