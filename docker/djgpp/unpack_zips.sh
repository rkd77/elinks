cd
mkdir -p lib/pkgconfig
mkdir zip
cd zip
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/expat20br2.zip
#wget http://ftp.delorie.com/pub/djgpp/current/v2tk/wat3211b.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/zlb13b.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/xz-525a.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2apps/bz2-108a.zip
wget https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/apps/sqlite.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/licv116b.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/lus0910b.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2tk/lua522b.zip
wget http://ftp.delorie.com/pub/djgpp/current/v2gnu/rdln80b.zip
wget http://mik.dyndns.pro/dos-stuff/bin/misc-dev.7z

mkdir tmp; unzip expat20br2.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
#mkdir tmp; unzip wat3211b.zip -d tmp; mv -f tmp/net/watt/inc tmp/net/watt/include; cp -a tmp/net/watt/include $HOME/; cp -a tmp/net/watt/lib $HOME/; rm -rf tmp
mkdir tmp; unzip zlb13b.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; unzip xz-525a.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; unzip bz2-108a.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; unzip sqlite.zip -d tmp; mkdir tmp/tmp2; unzip tmp/SOURCE/SQLITE/SOURCES.ZIP -d tmp/tmp2; cp -a tmp/tmp2/examples/sqlite3.h $HOME/include/; cp -a tmp/tmp2/examples/libsqlite3.a $HOME/lib/
cp -a tmp/tmp2/sqlite3.pc $HOME/lib/pkgconfig/; rm -rf tmp
mkdir tmp; unzip licv116b.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; unzip lus0910b.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; unzip rdln80b.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
mkdir tmp; cd tmp; 7za x -y ../misc-dev.7z; cd ..; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp

# remove to not break compilation
rm -rf $HOME/include/winsock2.h $HOME/include/ws2tcpip.h

sed -i -e 's|/dev/env/DJDIR|/home/elinks|g' $HOME/lib/pkgconfig/*.pc
sed -i -e 's|/dev/env/DJDIR|/home/elinks|g' $HOME/lib/*.la
sed -i -e 's/Libs\.private/#Libs.private/' $HOME/lib/pkgconfig/sqlite3.pc
mkdir tmp; unzip lua522b.zip -d tmp; cp -a tmp/include $HOME/; cp -a tmp/lib $HOME/; rm -rf tmp
