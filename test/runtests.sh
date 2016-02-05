#!/bin/bash
# runtests.sh
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

if [ -z $DISPLAY ]; then
  echo "X not detected."
  echo "Testing basic shell functions without Vim..."
  bash testscript.sh "$1" shell "Shell" || exit 1
  echo "Athame successfully falls back to normal shell behavior without X."
  echo "But Athame needs X for Vim functionality so the Vim tests weren't run."
  echo "If you want to install anyway, run this script again with --notest --dirty"
  if [ "$(uname)" == "Darwin" ]; then
    echo "You should install XQuartz to get X on OSX."
  fi
  exit 1
fi
bash testscript.sh "$1" shell "Shell" || exit 1
bash testscript.sh "$1" vim "Vim Integration" || exit 1
temp=$DISPLAY
unset DISPLAY
bash testscript.sh "$1" shell "Shell Fallback without X" || exit 1
DISPLAY=$temp
