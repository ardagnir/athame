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
redownload=0
build=1
athame=1
dirty=0
for arg in "$@"
do
  case $arg in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--noathame" ) athame=0;;
    "--dirty" ) dirty=1;;
    "--help" ) echo -e " --redownload: redownload readline and patches\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--noathame: setup normal readline without athame\n" \
                        "--dirty: don't run the whole build process,\n" \
                        "         just make and install changes\n" \
                        "         (only use after a successful build)\n" \
                        "--help: display this message"; exit;;
  esac
done

#Download Readline
if [ $redownload = 1 ]; then
  rm -r readline-6.3.tar.gz
fi
if [ ! -f readline-6.3.tar.gz ]; then
  wget https://ftp.gnu.org/gnu/readline/readline-6.3.tar.gz
fi

mkdir -p readline_patches
cd readline_patches
for (( patch=1; patch <= patches; patch++ )); do
  if [ $redownload = 1 ]; then
    rm -r readline63-$(printf "%03d" $patch)
  fi
  if [ ! -f readline63-$(printf "%03d" $patch) ]; then
    wget https://ftp.gnu.org/gnu/readline/readline-6.3-patches/readline63-$(printf "%03d" $patch)
  fi
done
cd ..

#Unpack readline dir
if [ $dirty = 0 ]; then
  rm -rf readline-6.3_tmp
  tar -xf readline-6.3.tar.gz
  mv readline-6.3 readline-6.3_tmp
fi

#Move into readline directory
cd readline-6.3_tmp

if [ $dirty = 0 ]; then
  #Patch readline with readline patches
  for (( patch=1; patch <= patches; patch++ )); do
    echo Patching with standard readline patch $patch
    patch -p0 < ../readline_patches/readline63-$(printf "%03d" $patch)
  done
fi

if [ $athame = 1 ]; then
  #Patch Readline with athame
  echo "Patching with athame patch"
  if [ $dirty = 0 ]; then
    patch -p1 < ../readline.patch
    cp -r ../vimbed .
  fi
  cp ../athame.* .
  cp ../athame_readline.h athame_intermediary.h
fi

#Build and install Readline
if [ $build = 1 ]; then
  if [ $dirty = 0 ]; then
    ./configure --prefix=/usr
  fi
  make SHLIB_LIBS=-lncurses
  sudo make install
fi

#Leave readline dir
cd ..
