cd
wget https://mujs.com/downloads/mujs-1.3.3.tar.gz
rm -rf mujs-1.3.3
tar -xf mujs-1.3.3.tar.gz
cd mujs-1.3.3
patch -p1 < ../mujs.diff
make CC=i586-pc-msdosdjgpp-gcc AR=i586-pc-msdosdjgpp-ar CFLAGS="-Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__" prefix=$HOME install
