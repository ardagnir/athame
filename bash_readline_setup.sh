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

patches=12
redownload=0
build=1
dirty=0
sudo="sudo "
destdir=""
prefix_flag="--prefix=/"
installed_flag="--with-installed-readline"
for arg in "$@"
do
  case "$arg" in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--dirty" ) dirty=1;;
    "--nosudo" ) sudo="";;
    --destdir=*) destdir="${arg#*=}";;
    --prefix=*) prefix_flag='--prefix='"${arg#*=}";;
    --with-installed-readline=*) installed_flag='--with-installed-readline='"${arg#*=}";;
    "--help" ) echo -e " --redownload: redownload bash and patches\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--prefix: set prefix for configure\n"\
                        "--with-installed-readline: set where to look for readline\n"\
                        "--destdir: set DESTDIR for install\n"\
                        "--dirty: don't run the whole build process,\n" \
                        "         just make and install changes\n" \
                        "         (only use after a successful build)\n" \
                        "--help: display this message"; exit;;
    * ) echo Unknown flag "$arg" >&2; exit 1;;
  esac
done

#Download bash
if [ $redownload = 1 ]; then
  rm -r bash-4.4.tar.gz
fi
if [ ! -f bash-4.4.tar.gz ]; then
  curl -O https://ftp.gnu.org/gnu/bash/bash-4.4.tar.gz
fi

mkdir -p bash_patches
cd bash_patches
for (( patch=1; patch <= patches; patch++ )); do
  if [ $redownload = 1 ]; then
    rm -r bash44-$(printf "%03d" $patch)
  fi
  if [ ! -f bash44-$(printf "%03d" $patch) ]; then
    curl -O https://ftp.gnu.org/gnu/bash/bash-4.4-patches/bash44-$(printf "%03d" $patch)
  fi
done
cd ..

if [ ! -d bash-4.4_tmp ]; then
  dirty=0
fi

#Unpack bash dir
if [ $dirty = 0 ]; then
  rm -rf bash-4.4_tmp
  tar -xf bash-4.4.tar.gz
  mv bash-4.4 bash-4.4_tmp
fi

#Move into bash directory
cd bash-4.4_tmp

if [ $dirty = 0 ]; then
  #Patch bash with bash patches
  for (( patch=1; patch <= patches; patch++ )); do
    echo Patching with standard bash patch $patch
    patch -p0 < ../bash_patches/bash44-$(printf "%03d" $patch)
  done
fi

#Build and install bash
if [ $build = 1 ]; then
  if [ ! -f Makefile ]; then
    ac_cv_rl_version=7.0 ./configure \
                "$prefix_flag" \
                --docdir=/usr/share/doc/bash-4.4 \
                --without-bash-malloc \
                --enable-readline \
                "$installed_flag" \
                || exit 1
  fi
  make LOCAL_LIBS=-lutil
  if [ $? != 0 ]; then
    printf "\n\e[1;31mMake failed:\e[0m Are you sure you have readline 7 installed? readline_athame_setup.sh installs readline 7 patched with athame. You may want to run it first.\nThis may also fail if you have a versionless libreadline.so symlinked to libreadline.so.6\n"
    exit 1
  fi
  if [ -n "$destdir" ]; then
    mkdir -p "$destdir"
  fi
  if [ -w "$destdir" ]; then
    make install DESTDIR="$destdir"
  else
    ${sudo}make install DESTDIR="$destdir"
  fi
fi

#Leave bash dir
cd ..
