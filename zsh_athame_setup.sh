#!/bin/bash
# zsh_athame_setup.sh -- Full vim integration for your shell.
#
# Copyright (C) 2017 James Kolb
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
sudo=""
vimbin=""
destdir=""
docdir=""
htmldir=""
prefix="/usr"
for arg in "$@"
do
  case "$arg" in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--noathame" ) athame=0;;
    "--notest" ) runtest=0;;
    "--dirty" ) dirty=1;;
    "--norc" ) rc=0;;
    "--nosubmodule" ) submodule=0;;
    "--use_sudo") sudo="sudo ";;
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
                        "--use_sudo: use sudo for installation and copying athamerc\n" \
                        "additional flags: these flags are passed to configure\n" \
                        "    --prefix=\n" \
                        "    --libdir=\n" \
                        "    --docdir=\n" \
                        "    --htmldir=\n" \
                        "--help: display this message"; exit;;
    * ) echo Unknown flag "$arg" >&2; exit 1;;
  esac
done

prefix_flag="--prefix=$prefix"
libdir_flag="${libdir:+--libdir=$libdir}"
docdir_flag="--docdir=${docdir:-$prefix/share/doc/zsh}"
htmldir_flag="--htmldir=${htmldir:-$docdir/html}"

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
  if [ "$($testvim --version | grep -E '(\+job|nvim)')" ]; then
    vimbin="$testvim"
    echo "$vimbin probably has job support. Using $vimbin as vim binary."
    ATHAME_USE_JOBS_DEFAULT=1
  elif [ "$($testvim --version | grep +clientserver)" ]; then
    vimbin=$testvim
    echo "$vimbin probably has clientserver support. Using $vimbin as vim binary."
    ATHAME_USE_JOBS_DEFAULT=0
  else
    echo "$testvim does not appear to have clientserver support."
    echo $vimmsg
    exit
  fi
else
  if [ "$($vimbin --version | grep -E '(\+job|nvim)')" ]; then
    ATHAME_USE_JOBS_DEFAULT=1
  else
    ATHAME_USE_JOBS_DEFAULT=0
  fi
fi

if [ $submodule = 1 ]; then
  git submodule update --init
fi

#Download zsh
if [ $redownload = 1 ]; then
  rm -r zsh-5.7.1.tar.xz
fi
if [ ! -f zsh-5.7.1.tar.xz ]; then
  curl -O https://www.zsh.org/pub/zsh-5.7.1.tar.xz
  head -n 3 zsh-5.7.1.tar.xz | grep "404 Not Found" > /dev/null
  if [ $? -eq 0 ]; then
    curl -O https://www.zsh.org/pub/old/zsh-5.7.1.tar.xz
  fi
fi
if [ "$(md5sum zsh-5.7.1.tar.xz 2>/dev/null)" != "374f9fdd121b5b90e07abfcad7df0627  zsh-5.7.1.tar.xz" ] && [ "$(md5 zsh-5.7.1.tar.xz 2>/dev/null)" != "MD5 (zsh-5.7.1.tar.xz) = 374f9fdd121b5b90e07abfcad7df0627" ]; then
  #rm zsh-5.7.1.tar.xz
  echo "FAILED: Incorrect md5 hash" >&2
  exit 1
fi

if [ ! -d zsh-5.7.1_tmp ]; then
  dirty=0
fi

#Unpack zsh dir
if [ $dirty = 0 ]; then
  rm -rf zsh-5.7.1_tmp
  tar -xf zsh-5.7.1.tar.xz
  mv zsh-5.7.1 zsh-5.7.1_tmp
fi

#Patch Zsh with Athame
cd zsh-5.7.1_tmp
if [ $athame = 1 ]; then
  if [ $dirty = 0 ]; then
    ../athame_patcher.sh zsh .. || exit 1
  else
    ../athame_patcher.sh --dirty zsh .. || exit 1
  fi
fi

if [ $build != 1 ]; then
  exit 0
fi

#Build and install zsh
if [ ! -f Makefile ]; then
  ./configure "$prefix_flag" \
      "$libdir_flag" \
      "$docdir_flag" \
      "$htmldir_flag" \
      --enable-etcdir=/etc/zsh \
      --enable-zshenv=/etc/zsh/zshenv \
      --enable-zlogin=/etc/zsh/zlogin \
      --enable-zlogout=/etc/zsh/zlogout \
      --enable-zprofile=/etc/zsh/zprofile \
      --enable-zshrc=/etc/zsh/zshrc \
      --enable-maildir-support \
      --enable-multibyte \
      --enable-function-subdirs \
      --enable-fndir="$prefix/share/zsh/functions" \
      --enable-scriptdir="$prefix/share/zsh/scripts" \
      --with-tcsetpgrp \
      --enable-pcre \
      --enable-cap \
      --enable-zsh-secure-free \
      || exit 1
fi
if [ $runtest = 1 ]; then
  rm -rf "$(pwd)/../test/build"
  mkdir -p "$(pwd)/../test/build"

  # make sure the files affected by ATHAME_TESTDIR are updated to use test settings
  rm -f Src/zshpaths.h && touch Src/Zle/athame.c

  make CFLAGS=-std=c99 ATHAME_VIM_BIN="$vimbin" ATHAME_USE_JOBS_DEFAULT="$ATHAME_USE_JOBS_DEFAULT" ATHAME_TESTDIR="$(pwd)/../test/build" || exit 1
  make install DESTDIR="$(pwd)/../test/build" || exit 1

  # make sure the files affected by ATHAME_TESTDIR are updated to not use test settings
  rm -f Src/zshpaths.h && touch Src/Zle/athame.c

  cd ../test
  zsh_bin="$(find $(pwd)/build -name zsh -type f | head -n 1)"
  if [ "$($vimbin --version | grep  nvim)" ]; then
    nvim="nvim"
  fi
  if [ "$(uname)" == "Darwin" ]; then
    ./runtests.sh "script /dev/null $zsh_bin" $nvim || exit 1
  else
    ./runtests.sh "script -c $zsh_bin" $nvim || exit 1
  fi
  cd -
fi
make CFLAGS=-std=c99 ATHAME_VIM_BIN="$vimbin" ATHAME_USE_JOBS_DEFAULT="$ATHAME_USE_JOBS_DEFAULT" || exit 1
echo "Installing Zsh with Athame..."
if [ -n "$destdir" ]; then
  mkdir -p "$destdir"
fi
${sudo}make install DESTDIR="$destdir" || exit 1

if [ $rc = 1 ]; then
    ${sudo}cp ../athamerc /etc/athamerc
    if [ $? -ne 0 ]; then
      printf "\e[0;31mThe athamerc was not copied. You should copy athamerc to /etc/athamerc or ~/.athamerc.\e[0;0m\n"
    fi
fi
