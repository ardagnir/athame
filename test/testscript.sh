#!/bin/bash
# testscript.sh
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

#TODO: ATHAME_TEST_RC isn't used yet
ATHAME_TEST_RC=$(pwd)/../../athamerc
echo "Testing Athame $3..."
mkdir -p testrun
rm -rf testrun/*
cp $2/inst* testrun
failures=""
cd testrun
for t in inst*.sh; do
  i=${t:4: -3}
  echo "Test $i:"
  # If we just pipe the text directly, Vim gets behind and athame times it out.
  # Instead we use charread to simulate typing at 50 char/sec.
  # This actually ends up being closer to 25 char/sec on my crappy laptop because
  # of overhead in charread, but that is still faster than world-recod human typists.
  ../charread.sh .02 inst$i.sh | $1 >/dev/null 2>&1
  diff ../$2/expected$i out$i >/dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo "Success!"
  else
      echo "Failed"
      failures="$failures inst$i.sh"
  fi
  echo ""
done
cd ..
if [[ -n $failures ]]; then
  echo "Test failed"
  echo "Failed tests:$failures"
  echo 'use `cd test/testrun; ../charread.sh .01 $test_name | '$1'` to view the failure'
  exit 1
fi
exit 0
