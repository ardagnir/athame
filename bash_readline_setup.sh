#!/bin/bash
# bash_readline_setup.sh -- Full vim integration for your shell.
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

patches=42
redownload=0
build=1
dirty=0
for arg in "$@"
do
  case $arg in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--dirty" ) dirty=1;;
    "--help" ) echo -e " --redownload: redownload bash and patches\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--dirty: don't run the whole build process,\n" \
                        "         just make and install changes\n" \
                        "         (only use after a successful build)\n" \
                        "--help: display this message"; exit;;
  esac
done

#Download bash
if [ $redownload = 1 ]; then
  rm -r bash-4.3.tar.gz
fi
if [ ! -f bash-4.3.tar.gz ]; then
  wget https://ftp.gnu.org/gnu/bash/bash-4.3.tar.gz
fi

mkdir -p bash_patches
cd bash_patches
for (( patch=1; patch <= patches; patch++ )); do
  if [ $redownload = 1 ]; then
    rm -r bash43-$(printf "%03d" $patch)
  fi
  if [ ! -f bash43-$(printf "%03d" $patch) ]; then
    wget https://ftp.gnu.org/gnu/bash/bash-4.3-patches/bash43-$(printf "%03d" $patch)
  fi
done
cd ..

#Unpack bash dir
if [ $dirty = 0 ]; then
  rm -rf bash-4.3_tmp
  tar -xf bash-4.3.tar.gz
  mv bash-4.3 bash-4.3_tmp
fi

#Move into bash directory
cd bash-4.3_tmp

if [ $dirty = 0 ]; then
  #Patch bash with bash patches
  for (( patch=1; patch <= patches; patch++ )); do
    echo Patching with standard bash patch $patch
    patch -p0 < ../bash_patches/bash43-$(printf "%03d" $patch)
  done
fi

#Build and install bash
if [ $build = 1 ]; then
  if [ $dirty = 0 ]; then
    ./configure --prefix=/usr \
                --bindir=/bin \
                --docdir=/usr/share/doc/bash-4.3 \
                --without-bash-malloc \
                --enable-readline \
                --with-installed-readline=/usr
  fi
  make
  sudo make install
fi

#Leave bash dir
cd ..
