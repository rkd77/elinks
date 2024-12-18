cd
rm -rf quickjs-2024-01-13
wget https://bellard.org/quickjs/quickjs-2024-01-13.tar.xz
tar xf quickjs-2024-01-13.tar.xz
cp Makefile.win32 quickjs-2024-01-13/
cd quickjs-2024-01-13
make -f Makefile.win32 PREFIX=$HOME/64 CONFIG_WIN32=y
make -f Makefile.win32 install PREFIX=$HOME/64 CONFIG_WIN32=y
