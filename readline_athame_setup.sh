#!/bin/bash
# readline_athame_setup.sh -- Full vim integration for your shell.
#
# Copyright (C) 2017 James Kolb
#
# This file is part of Athame.
#
# Athame is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or # (at your option) any later version.
#
# Athame is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Athame.  If not, see <http://www.gnu.org/licenses/>.

shopt -s extglob

patches=3
redownload=0
build=1
runtest=1
athame=1
dirty=0
rc=1
submodule=1
vimbin=""
destdir=""
sudo="sudo "
prefix_flag="--prefix=/usr"
libdir_flag=""
for arg in "$@"
do
  case "$arg" in
    "--redownload" ) redownload=1;;
    "--nobuild" ) build=0;;
    "--notest" ) runtest=0;;
    "--noathame" ) athame=0;;
    "--dirty" ) dirty=1;;
    "--norc" ) rc=0;;
    "--nosubmodule" ) submodule=0;;
    "--nosudo" ) sudo="";;
    --vimbin=*) vimbin="${arg#*=}";;
    --destdir=*) destdir="${arg#*=}";;
    --prefix=*) prefix_flag='--prefix='"${arg#*=}";;
    --libdir=*) libdir="${arg#*=}";libdir_flag="--libdir=$libdir";;
    "--help" ) echo -e " --redownload: redownload readline and patches\n" \
                        "--nobuild: stop before actually building src\n" \
                        "--notest: don't run tests\n" \
                        "--noathame: setup normal readline without athame\n" \
                        "--vimbin=path/to/vim: set a path to the vim binary\n"\
                        "                      you want athame to use\n" \
                        "--prefix: set prefix for configure\n"\
                        "--libdir: set libdir for configure\n"\
                        "--destdir: set DESTDIR for install\n"\
                        "--dirty: don't run the whole patching/configure process,\n" \
                        "         just make and install changes\n" \
                        "--norc: don't copy the rc file to /etc/athamerc\n" \
                        "--nosubmodule: don't update submodules\n" \
                        "--help: display this message"; exit;;
    * ) echo Unknown flag "$arg" >&2; exit 1;;
  esac
done

#Get vim binary
if [ -z "$vimbin" ]; then
  vimmsg="Please provide a vim binary by running this script with --vimbin=/path/to/vim at the end. (replace with the actual path to vim)"
  testvim="$(which vim)"
  if [ -z "$testvim" ]; then
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
    vimbin="$testvim"
    echo "$vimbin probably has clientserver support. Using $vimbin as vim binary."
    ATHAME_USE_JOBS_DEFAULT=0
  else
    echo "$testvim does not appear to have job or clientserver support."
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

#Download Readline
if [ $redownload = 1 ]; then
  rm -r readline-7.0.tar.gz
fi
if [ ! -f readline-7.0.tar.gz ]; then
  curl -O https://ftp.gnu.org/gnu/readline/readline-7.0.tar.gz
fi

mkdir -p readline_patches
cd readline_patches
for (( patch=1; patch <= patches; patch++ )); do
  if [ $redownload = 1 ]; then
    rm -r readline70-$(printf "%03d" $patch)
  fi
  if [ ! -f readline70-$(printf "%03d" $patch) ]; then
    curl -O https://ftp.gnu.org/gnu/readline/readline-7.0-patches/readline70-$(printf "%03d" $patch)
  fi
done
cd ..

if [ ! -d readline-7.0_tmp ]; then
  dirty=0
fi

#Unpack readline dir
if [ $dirty = 0 ]; then
  rm -rf readline-7.0_tmp
  tar -xf readline-7.0.tar.gz
  mv readline-7.0 readline-7.0_tmp
fi

#Move into readline directory
cd readline-7.0_tmp

if [ $dirty = 0 ]; then
  #Patch readline with readline patches
  for (( patch=1; patch <= patches; patch++ )); do
    echo Patching with standard readline patch $patch
    patch -p0 < ../readline_patches/readline70-$(printf "%03d" $patch)
  done
fi

if [ $athame = 1 ]; then
  if [ $dirty = 0 ]; then
    ../athame_patcher.sh readline .. || exit 1
  else
    ../athame_patcher.sh --dirty readline .. || exit 1
  fi
fi

if [ $build != 1 ]; then
  exit 0
fi

#Build and install Readline
if [ ! -f Makefile ]; then
  ./configure "$prefix_flag" "$libdir_flag" || exit 1
fi
make CFLAGS=-std=c99 SHLIB_LIBS="-lncurses -lutil" ATHAME_VIM_BIN="$vimbin" ATHAME_USE_JOBS_DEFAULT="$ATHAME_USE_JOBS_DEFAULT" || exit 1
if [ $runtest = 1 ]; then
  rm -rf $(pwd)/../test/build
  mkdir -p $(pwd)/../test/build
  make install DESTDIR=$(pwd)/../test/build || exit 1

  cd ../test

  export LD_LIBRARY_PATH="$(dirname $(find $(pwd)/build -name libreadline* | head -n 1))"
  export ATHAME_VIMBED_LOCATION="$(find $(pwd)/build -name athame_readline | head -n 1)"

  if [ "$($vimbin --version | grep  nvim)" ]; then
    nvim="nvim"
  fi
  if [ "$(uname)" == "Darwin" ]; then
    export DYLD_LIBRARY_PATH="$LD_LIBRARY_PATH"
    otool -L "$(which bash)" | grep libreadline.7.dylib >/dev/null
  else
    ldd "$(which bash)" | grep libreadline.so.7 >/dev/null
  fi
  if [ $? -eq 1 ]; then
    echo "Bash isn't set to use system readline or is not using readline 7. Setting up local bash for testing."
    cd ..
    ./bash_readline_setup.sh --destdir="$(pwd)/test/build" --with-installed-readline="${LD_LIBRARY_PATH%+(/lib|/lib/*)}"
    cd test
    ./runtests.sh "$(pwd)/build/bin/bash -i" bash $nvim || exit 1
  else
    ./runtests.sh "bash -i" bash $nvim || exit 1
  fi
  cd ../readline-7.0_tmp
fi
echo "Installing Readline with Athame..."
if [ -n "$destdir" ]; then
  mkdir -p "$destdir"
fi
if [ -w "$destdir" ]; then
  make install DESTDIR="$destdir" || exit 1
else
  ${sudo}make install DESTDIR="$destdir" || exit 1
fi

if [ $rc = 1 ]; then
  if [ -z "$sudo" ]; then
    printf "\e[0;31mThe athamerc was not copied. You should copy athamerc to /etc/athamerc or ~/.athamerc.\e[0;0m\n"
  else
    sudo cp ../athamerc /etc/athamerc
  fi
fi
