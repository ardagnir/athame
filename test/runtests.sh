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
sanity=0

function runtest () {
  sanity=$((sanity-1))
  export ATHAME_TEST_RC=$(pwd)/../athamerc
  echo "Testing Athame $3..."
  mkdir -p testrun
  rm -rf testrun/*
  cp $2/inst* testrun
  failures=""
  cd testrun
  for t in inst*.sh; do
    i=${t:4:${#t}-7}
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
        script -c "../charread.sh .15 input_text | $1" failure > /dev/null
        diff ../$2/expected$i out$i >>failure 2>&1
        if [ $? -eq 0 ]; then
          echo "Success!"
          slow=1
        else
          echo "Failed at slow speed."
          cat failure >>failures
          failures="$failures $i"
        fi
    fi
    echo ""
  done
  cd ..
  if [[ -n $failures ]]; then
    echo "Test Failed"
    echo "Failed tests:$failures"
    while true; do
      echo -e "\n\e[1mWhat now?\e[0m\n\e[1mv:\e[0m view failures\n\e[1mC:\e[0m \e[0;31m*DANGEROUS*\e[0;0m continue anyway\n\e[1mx:\e[0m exit"
      read -rn 1
      echo ""
      if [[ $REPLY =~ ^[Vv]$ ]]; then
        cat testrun/failures
      elif [[ $REPLY =~ ^[C]$ ]]; then
        sanity=$((sanity+1))
        return 1
      elif [[ $REPLY =~ ^[Xx]$ ]]; then
        exit 1
      else
        echo -e "Invalid option"
      fi
    done
  fi
  sanity=$((sanity+1))
  return 0
}

if [ -z $DISPLAY ]; then
  echo "X not detected."
  echo "Testing basic shell functions without Vim..."
  runtest "$1" shell "Shell" &&
    echo "Athame successfully falls back to normal shell behavior without X. But "
  echo "Athame needs X for Vim functionality so the Vim tests weren't run."
  if [ "$(uname)" == "Darwin" ]; then
    echo "You should install XQuartz to get X on OSX."
  fi
  read -p "Install anyway? (y:yes, other:no)? " -rn 1
  echo ""
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    exit 0
  fi
  exit 1
fi

runtest "$1" shell "Shell"
runtest "$1" vim "Vim Integration"
temp=$DISPLAY
unset DISPLAY
runtest "$1" shell "Shell Fallback without X"
DISPLAY=$temp

if [ $slow -eq 1 ]; then
  echo "Test Result: Athame is running slow on this computer."
  read -p "Install anyway? (y:yes, other:no)? " -rn 1
  if ! [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    exit 1
  fi
  echo ""
fi

# Bash errors can cause us to skip code. Make extra sure we've run the tests if 
# we're saying we passed.
if [ $sanity -lt 0 ]; then
  exit 1
fi
