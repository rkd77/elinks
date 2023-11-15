cd
rm -rf quickjs-2021-03-27
wget https://bellard.org/quickjs/quickjs-2021-03-27.tar.xz
tar xf quickjs-2021-03-27.tar.xz
cd quickjs-2021-03-27
patch -p1 < ../quickjs-dos.diff
make -f Makefile.dos prefix=$HOME
make -f Makefile.dos install prefix=$HOME
