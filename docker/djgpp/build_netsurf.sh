cd
wget http://download.netsurf-browser.org/libs/releases/buildsystem-1.10.tar.gz
rm -rf buildsystem-1.10
tar -xf buildsystem-1.10.tar.gz
make -C buildsystem-1.10 install PREFIX=$HOME
