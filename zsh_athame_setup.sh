#!/bin/sh
# zsh_athame_setup.sh -- Full vim integration for your shell.
#
# Copyright (C) 2015 James Kolb
#
# This file is part of Athame.
#
# Athame is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Athame is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Athame.  If not, see <http://www.gnu.org/licenses/>.

#Download zsh
if [ ! -f zsh-5.0.8.tar.bz2 ]; then
  wget http://www.zsh.org/pub/zsh-5.0.8.tar.bz2
  if [ "$(md5sum zsh-5.0.8.tar.bz2)" != "e6759e8dd7b714d624feffd0a73ba0fe  zsh-5.0.8.tar.bz2" ]; then
    rm zsh-5.0.8.tar.bz2
    echo "FAILED: Incorrect md5 hash" >&2
    exit 1
  fi
fi

#Unpack zsh dir
rm -rf zsh-5.0.8_tmp
tar -xf zsh-5.0.8.tar.bz2
mv zsh-5.0.8 zsh-5.0.8_tmp

#Patch zsh
cd zsh-5.0.8_tmp/Src
patch -p1 < ../../zsh.patch
cp ../../athame.* Zle/
cp ../../athame_zsh.h Zle/athame_intermediary.h
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
