#!/bin/sh
# readline_athame_setup.sh -- Full vim integration for your shell.*/
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
