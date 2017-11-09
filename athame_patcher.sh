#!/bin/bash
# athame_patcher.sh -- Full vim integration for your shell.
#
# Copyright (C) 2015 James Kolb
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

if [ "$1" == "--help" ]; then
  echo "Usage athame_patcher.sh [ --dirty ] ( readline | zsh ) athame_dir"
  exit 0
fi

if [ "$1" == "--dirty" ]; then
  dirty=1
  ptype="$2"
  adir="$3"
else
  dirty=0
  ptype="$1"
  adir="$2"
fi

if [ "$ptype" != "zsh" ] && [ "$ptype" != "readline" ]; then
  echo Invalid patch "$ptype" >&2
  exit 1
fi

if [ -z "$adir" ]; then
  echo No directory provided
  exit 1
fi

if [ $dirty = 0 ]; then
  echo "Patching with Athame patch"
  patch -p1 < "$adir/$ptype.patch"
fi

sdir="."
if [ "$ptype" = zsh ]; then
  sdir="./Src/Zle"
fi

rm -rf "$sdir/vimbed"
cp -r "$adir/vimbed" "$sdir"
echo "Copying Athame files"
cp "$adir/athame".* "$sdir"
cp "$adir/athame_util.h" "$sdir"
cp "$adir/athame_$ptype.h" "$sdir/athame_intermediary.h"
