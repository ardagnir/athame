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
runtest=1
athame=1
dirty=0
rc=1
submodule=1
vimbin=""
destdir=""
prefix="/usr"
for arg in "$@"
do
  case $arg in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--noathame" ) athame=0;;
    "--notest" ) runtest=0;;
    "--dirty" ) dirty=1;;
    "--norc" ) rc=0;;
    "--nosubmodule" ) submodule=0;;
    --vimbin=*) vimbin="${arg#*=}";;
    --destdir=*) destdir="${arg#*=}";;
    --prefix=*) prefix="${arg#*=}";;
    --libdir=*) libdir="${arg#*=}";;
    --docdir=*) docdir="${arg#*=}";;
    --htmldir=*) htmldir="${arg#*=}";;
    "--help" ) echo -e " --redownload: redownload zsh\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--notest: don't run tests\n" \
                        "--noathame: setup normal zsh without athame\n" \
                        "--vimbin=path/to/vim: set a path to the vim binary\n"\
                        "                      you want athame to use\n" \
                        "--destdir: set DESTDIR for install\n"\
                        "--dirty: don't run the whole patching/configure process,\n" \
                        "         just make and install changes\n" \
                        "--norc: don't copy the rc file to /etc/athamerc\n" \
                        "--nosubmodule: don't update submodules\n" \
                        "additional flags: these flags are passed to configure\n" \
                        "    --prefix=\n" \
                        "    --libdir=\n" \
                        "    --docdir=\n" \
                        "    --htmldir=\n" \
                        "--help: display this message"; exit;;
  esac
done

prefix_flag="--prefix=$prefix"
libdir_flag=${libdir:+"--libdir=$libdir"}
docdir_flag="--docdir=${docdir-$prefix/share/doc/zsh}"
htmldir_flag="--htmldir=${htmldir-$docdir/html}"

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

if [ $submodule = 1 ]; then
  git submodule update --init
fi

#Download zsh
if [ $redownload = 1 ]; then
  rm -r zsh-5.k.1.tar.gz
fi
if [ ! -f zsh-5.1.1.tar.gz ]; then
  curl -O http://www.zsh.org/pub/old/zsh-5.1.1.tar.gz
fi
if [ "$(md5sum zsh-5.1.1.tar.gz 2>/dev/null)" != "8ba28a9ef82e40c3a271602f18343b2f  zsh-5.1.1.tar.gz" ] && [ "$(md5 zsh-5.1.1.tar.gz 2>/dev/null)" != "MD5 (zsh-5.1.1.tar.gz) = 8ba28a9ef82e40c3a271602f18343b2f" ]; then
  rm zsh-5.1.1.tar.gz
  echo "FAILED: Incorrect md5 hash" >&2
  exit 1
fi

if [ ! -d zsh-5.1.1_tmp ]; then
  dirty=0
fi

#Unpack zsh dir
if [ $dirty = 0 ]; then
  rm -rf zsh-5.1.1_tmp
  tar -xf zsh-5.1.1.tar.gz
  mv zsh-5.1.1 zsh-5.1.1_tmp
fi

#Patch Zsh with Athame
cd zsh-5.1.1_tmp
if [ $athame = 1 ]; then
  if [ $dirty = 0 ]; then
    patch -p1 < ../zsh.patch
  fi
  rm -rf Src/vimbed
  cp -r ../vimbed Src/
  cp ../athame.* Src/Zle/
  cp ../athame_util.h Src/Zle/
  cp ../athame_zsh.h Src/Zle/athame_intermediary.h
fi

#Build and install zsh
if [ $build = 1 ]; then
  if [ ! -f Makefile ]; then
    ./configure $prefix_flag \
        $docdir_flag \
        $htmldir_flag \
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
        --enable-zsh-secure-free \
        || exit 1
  fi
  if [ $runtest = 1 ]; then
    rm -rf $(pwd)/../test/build
    mkdir -p $(pwd)/../test/build

    # make sure the files affected by ATHAME_TESTDIR are updated to use test settings
    rm -f Src/zshpaths.h && touch Src/Zle/athame.c

    mkdir -p $(pwd)/../test/build/usr/lib
    make ATHAME_VIM_BIN=$vimbin ATHAME_TESTDIR=$(pwd)/../test/build || exit 1
    make install DESTDIR=$(pwd)/../test/build || exit 1

    # make sure the files affected by ATHAME_TESTDIR are updated to not use test settings
    rm -f Src/zshpaths.h && touch Src/Zle/athame.c

    cd ../test
    ./runtests.sh "script -c ../build/usr/bin/zsh" || exit 1
    cd -
    make ATHAME_VIM_BIN=$vimbin || exit 1
    echo "Installing Zsh with Athame..."
    if [ -n "$destdir" ]; then
      mkdir -p $destdir
    fi
    if [ -w "$destdir" ]; then
      make install DESTDIR=$destdir || exit 1
    else
      sudo make install DESTDIR=$destdir || exit 1
    fi
  else
    make ATHAME_VIM_BIN=$vimbin || exit 1
    echo "Installing Zsh with Athame..."
    if [ -n "$destdir" ]; then
      mkdir -p $destdir
    fi
    if [ -w "$destdir" ]; then
      make install DESTDIR=$destdir || exit 1
    else
      sudo make install DESTDIR=$destdir || exit 1
    fi
  fi
fi

if [ $rc = 1 ]; then
  sudo cp ../athamerc /etc/athamerc
fi
