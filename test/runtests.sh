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

slow=0
function runtest () {
  export ATHAME_TEST_RC=$(pwd)/../athamerc
  echo "Testing Athame $3..."
  mkdir -p testrun
  rm -rf testrun/*
  cp $2/inst* testrun
  failures=""
  cd testrun
  for t in inst*.sh; do
    i=${t:4: -3}
    echo "Test $i:"
    cat ../prefix.sh inst$i.sh | grep -v '^\#' > input_text
    # If we just pipe the text directly, Vim gets behind and athame times it out.
    # Instead we use charread to simulate typing at 33 char/sec.
    # This actually ends up being closer to 25-30 char/sec on my crappy laptop because
    # of overhead in charread, but that is still faster than world-recod human typists.
    script -c "../charread.sh .03 input_text | $1" failure > /dev/null
    diff ../$2/expected$i out$i >>failure 2>&1
    if [ $? -eq 0 ]; then
      echo "Success!"
    else
        echo "Failed at high speed. Retrying at slower speed"
        slow=1
        script -c "../charread.sh .15 input_text | $1" failure > /dev/null
        diff ../$2/expected$i out$i >>failure 2>&1
        if [ $? -eq 0 ]; then
          echo "Success!"
        else
          echo "Failed at slow speed."
          cat failure >>failures
          failures="$failures $i"
        fi
    fi
    echo ""
  done
  if [[ -n $failures ]]; then
    echo "Test Failed"
    echo "Failed tests:$failures"
    read -p "View failures (y:yes, other:no)? " -rn 1
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      cat failures
    else
      echo ""
    fi
    exit 1
  fi
  cd ..
}

if [ -z $DISPLAY ]; then
  echo "X not detected."
  echo "Testing basic shell functions without Vim..."
  runtest "$1" shell "Shell"
  echo "Athame successfully falls back to normal shell behavior without X."
  echo "But Athame needs X for Vim functionality so the Vim tests weren't run."
  echo "If you want to install anyway, run this script again with --notest --dirty"
  if [ "$(uname)" == "Darwin" ]; then
    echo "You should install XQuartz to get X on OSX."
  fi
  exit 1
fi

runtest "$1" shell "Shell"
runtest "$1" vim "Vim Integration"
temp=$DISPLAY
unset DISPLAY
runtest "$1" shell "Shell Fallback without X"
DISPLAY=$temp

if [[ $slow == 1 ]]; then
  echo "Test Result: Athame is running slow on this computer."
  read -p "Pass anyway? (y:yes, other:no)? " -rn 1
  if ! [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    exit 1
  fi
  echo ""
fi
