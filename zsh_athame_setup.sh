#!/bin/sh
#Download zsh
if [ ! -f zsh-5.0.8.tar.bz2 ]; then
  wget http://www.zsh.org/pub/zsh-5.0.8.tar.bz2
fi

#Unpack zsh dir
rm -rf zsh-5.0.8_tmp
tar -xf zsh-5.0.8.tar.bz2
mv zsh-5.0.8 zsh-5.0.8_tmp

#Patch zsh
cd zsh-5.0.8_tmp/Src
patch -p1 < ../../zsh.patch
cp ../../athame* Zle/
cp -r ../../vimbed .
cd ..

#Build and install zsh
./configure --prefix=/usr \
    --docdir=/usr/share/doc/zsh \
    --htmldir=/usr/share/doc/zsh/html \
    --enable-etcdir=/etc/zsh \
    --enable-zshenv=/etc/zsh/zshenv \
    --enable-zlogin=/etc/zsh/zlogin \
    --enable-zlogout=/etc/zsh/zlogout \
    --enable-zprofile=/etc/zsh/zprofile \
    --enable-zshrc=/etc/zsh/zshrc \
    --enable-maildir-support \
    --with-term-lib='ncursesw' \
    --enable-multibyte \
    --enable-function-subdirs \
    --enable-fndir=/usr/share/zsh/functions \
    --enable-scriptdir=/usr/share/zsh/scripts \
    --with-tcsetpgrp \
    --enable-pcre \
    --enable-cap \
    --enable-zsh-secure-free
make
sudo make install
cd ..
