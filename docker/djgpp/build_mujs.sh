cd
wget https://mujs.com/downloads/mujs-1.3.4.tar.gz
rm -rf mujs-1.3.4
tar -xf mujs-1.3.4.tar.gz
cd mujs-1.3.4
patch -p1 < ../mujs.diff
make CC=i586-pc-msdosdjgpp-gcc AR=i586-pc-msdosdjgpp-ar HAVE_READLINE=no CFLAGS="-Wno-error -DWATT32_NO_NAMESPACE -U__STRICT_ANSI__" prefix=$HOME install
