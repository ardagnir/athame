#!/bin/sh
# readline_athame_setup.sh -- Full vim integration for your shell.
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

patches=8

#Download Readline
if [ ! -f readline-6.3.tar.gz ]; then
  wget https://ftp.gnu.org/gnu/readline/readline-6.3.tar.gz
fi

mkdir -p readline_patches
cd readline_patches
for (( patch=1; patch <= patches; patch++ )); do
  if [ ! -f readline63-$(printf "%03d" $patch) ]; then
    wget https://ftp.gnu.org/gnu/readline/readline-6.3-patches/readline63-$(printf "%03d" $patch)
  fi
done
cd ..

#Unpack readline dir
rm -rf readline-6.3_tmp
tar -xf readline-6.3.tar.gz
mv readline-6.3 readline-6.3_tmp

#Move into readline directory
cd readline-6.3_tmp

#Patch readline with readline patches
for (( patch=1; patch <= patches; patch++ )); do
  echo Patching with standard readline patch $patch
  patch -p0 < ../readline_patches/readline63-$(printf "%03d" $patch)
done

#Patch Readline with athame
echo "Patching with athame patch"
patch -p1 < ../readline.patch
cp ../athame.* .
cp ../athame_readline.h athame_intermediary.h
cp -r ../vimbed .

#Build and install Readline
./configure --prefix=/usr
make SHLIB_LIBS=-lncurses
sudo make install

#Leave readline dir
cd ..
