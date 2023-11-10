cd
wget https://mujs.com/downloads/mujs-1.3.3.tar.gz
rm -rf mujs-1.3.3
tar -xf mujs-1.3.3.tar.gz
cd mujs-1.3.3
patch -p1 < ../mujs.diff
make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar prefix=$HOME install
