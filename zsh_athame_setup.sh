#!/bin/bash
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

redownload=0
build=1
athame=1
dirty=0
rc=1
vimbin=""
for arg in "$@"
do
  case $arg in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--noathame" ) athame=0;;
    "--dirty" ) dirty=1;;
    "--norc" ) rc=0;;
    --vimbin=*) vimbin="${arg#*=}";;
    "--help" ) echo -e " --redownload: redownload zsh\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--noathame: setup normal zsh without athame\n" \
                        "--dirty: don't run the whole build process,\n" \
                        "         just make and install changes\n" \
                        "         (only use after a successful build)\n" \
                        "--norc: don't copy the rc file to /etc/athamerc\n" \
                        "--help: display this message"; exit;;
  esac
done

#Get vim binary
if [ -z $vimbin ]; then
  vimmsg="Please provide a vim binary by running this script with --vimbin=/path/to/vim at the end. (replace with the actual path to vim)"
  testvim=$(which vim)
  if [ -z $testvim ]; then
    echo "Could not find a vim binary using 'which'"
    echo $vimmsg
    exit
  fi
  echo "No vim binary provided. Trying $testvim"
  if [ "$($testvim --version | grep +clientserver)" ]; then
    vimbin=$testvim
    echo "$vimbin probably has clientserver support. Using $vimbin as vim binary."
  else
    echo "$testvim does not appear to have clientserver support."
    echo $vimmsg
    exit
  fi
fi

#Download zsh
if [ $redownload = 1 ]; then
  rm -r zsh-5.0.8.tar.bz2
fi
if [ ! -f zsh-5.0.8.tar.bz2 ]; then
  wget http://www.zsh.org/pub/old/zsh-5.0.8.tar.bz2
  if [ "$(md5sum zsh-5.0.8.tar.bz2)" != "e6759e8dd7b714d624feffd0a73ba0fe  zsh-5.0.8.tar.bz2" ]; then
    rm zsh-5.0.8.tar.bz2
    echo "FAILED: Incorrect md5 hash" >&2
    exit 1
  fi
fi

#Unpack zsh dir
if [ $dirty = 0 ]; then
  rm -rf zsh-5.0.8_tmp
  tar -xf zsh-5.0.8.tar.bz2
  mv zsh-5.0.8 zsh-5.0.8_tmp
fi

#Patch Zsh with Athame
cd zsh-5.0.8_tmp
if [ $athame = 1 ]; then
  if [ $dirty = 0 ]; then
    patch -p1 < ../zsh.patch
    cp -r ../vimbed Src/
  fi
  cp ../athame.* Src/Zle/
  cp ../athame_zsh.h Src/Zle/athame_intermediary.h
fi

#Build and install zsh
if [ $build = 1 ]; then
  if [ $dirty = 0 ]; then
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
  fi
  make ATHAME_VIM_BIN=$vimbin
  sudo make install
fi

if [ $rc = 1 ]; then
  sudo cp ../athamerc /etc/athamerc
fi

#Leave the zsh dir
cd ..
