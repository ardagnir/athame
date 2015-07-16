#!/bin/sh
#Download Readline
if [ ! -f readline-6.3.tar.gz ]; then
  wget https://ftp.gnu.org/gnu/readline/readline-6.3.tar.gz
fi

#Unpack readline dir
rm -rf readline-6.3_tmp
tar -xf readline-6.3.tar.gz
mv readline-6.3 readline-6.3_tmp

#Patch Readline
cd readline-6.3_tmp
patch -p1 < ../readline.patch
cp ../athame* .
cp -r ../vimbed .

#Build and install Readline
./configure --prefix=/usr
make SHLIB_LIBS=-lncurses
sudo make install
cd ..
