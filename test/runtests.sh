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

first_test=1
slow=0
sanity=0

function runtest () {
  sanity=$((sanity-1))
  export ATHAME_TEST_RC=$(pwd)/../athamerc
  echo "Testing Athame $3..."
  mkdir -p testrun
  rm -rf testrun/*
  cp "$2"/inst* testrun
  failures=""
  cd testrun
  for t in inst*.sh; do
    i=${t:4:${#t}-7}
    echo "Test $i:"
    cat ../prefix.sh inst$i.sh | grep -v '^\#' > input_text
    printf "exit\n\x04" >> input_text

    milli=1
    # Do we have millisecond precision in date?
    if [ $(date +%s%3N | grep N) ]; then
      milli=0
    fi
    if [ $milli != 0 ]; then
      start_time=$(date +%s%3N)
    else
      start_time=$(date +%s999)
    fi
    script -c "cat input_text | $1" failure > /dev/null 2> /dev/null
    if [ $? -ne 0 ]; then
      # Linux version failed. Try bsd version:
      script failure bash -c "cat input_text | $1" > /dev/null
    fi
    if [ $milli != 0 ]; then
      end_time=$(date +%s%3N)
    else
      end_time=$(date +%s000)
      if [ $end_time -lt $start_time ]; then
        end_time=$((start_time+1))
      fi
    fi
    keys_typed=$(($(wc -c < input_text)))
    speed=$((keys_typed * 1000 / $((end_time-start_time))))
    # Make sure we can handle at least 27 keys per second. This is about 294
    # words per minute for English text, faster than world record typists.
    #
    # We don't count the first test for speed. We already know that vim can take
    # a while to load off disk on the first run and we don't want to mark the
    # test as slow just because the user hasn't run vim yet.
    if [ $milli != 0 ]; then
      echo speed=$speed
    fi
    if [ $first_test -ne 1 ] && [ $speed -lt 27 ]; then
      slow=1
      if [ $milli == 0 ]; then
        echo speed=$speed
      fi
    fi
    diff "../$2/expected$i" out$i >>failure 2>&1
    if [ $? -eq 0 ]; then
      echo "Success!"
    else
      echo "Failed."
      cat failure >>failures
      failures="$failures $i"
    fi
    echo ""
    first_test=0
  done
  cd ..
  if [[ -n $failures ]]; then
    echo "Test Failed"
    echo "Failed tests:$failures"
    while true; do
      printf "\n\e[1mWhat now?\e[0m\n\e[1mv:\e[0m view failures\n\e[1mC:\e[0m \e[0;31m*DANGEROUS*\e[0;0m continue anyway\n\e[1mx:\e[0m exit\n"
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
        echo "Invalid option"
      fi
    done
  fi
  sanity=$((sanity+1))
  return 0
}

if [ -z "$DISPLAY" ]; then
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
if [ "$2" == "bash" ]; then
  runtest "$1" bash "Bash Shell"
fi
runtest "$1" vim "Vim Integration"
temp=$DISPLAY
unset DISPLAY
runtest "$1" shell "Shell without X"
if [ "$2" == "bash" ]; then
  runtest "$1" bash "Bash Shell without X"
fi
DISPLAY=$temp

if [ $slow -eq 1 ]; then
  echo "Athame is running slow on this computer. Install anyway?"
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
