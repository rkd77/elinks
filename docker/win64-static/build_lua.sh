cd
wget https://www.lua.org/ftp/lua-5.4.6.tar.gz
rm -rf lua-5.4.6
tar -xf lua-5.4.6.tar.gz
cd lua-5.4.6
patch -p1 < ../lua.diff
make mingw CC="x86_64-w64-mingw32-gcc" AR="x86_64-w64-mingw32-ar rcu" RANLIB="x86_64-w64-mingw32-ranlib" INSTALL_TOP=$HOME -j $(nproc)
make install CC="x86_64-w64-mingw32-gcc" AR="x86_64-w64-mingw32-ar rcu" RANLIB="x86_64-w64-mingw32-ranlib" INSTALL_TOP=$HOME
