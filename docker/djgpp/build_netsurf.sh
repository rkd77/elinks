cd
wget http://download.netsurf-browser.org/libs/releases/buildsystem-1.9.tar.gz
rm -rf buildsystem-1.9
tar -xf buildsystem-1.9.tar.gz
make -C buildsystem-1.9 install PREFIX=$HOME
