cd
wget https://mujs.com/downloads/mujs-1.3.4.tar.gz
rm -rf mujs-1.3.4
tar -xf mujs-1.3.4.tar.gz
cd mujs-1.3.4
patch -p1 < ../mujs.diff
make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar HAVE_READLINE=no prefix=$HOME install
