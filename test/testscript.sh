#!/bin/bash
mkdir -p testrun
rm -rf testrun/*
cp inst* testrun
failures=""
cd testrun
for (( i = 1; i < 3; i++ ));do
  echo "Test $i:"
  bash -i < inst$i.sh 2>/dev/null
  diff ../expected$i out$i 2>&1 >/dev/null && echo "Success!" || (echo "Failed" && failures="$failures inst$i.sh")
  echo ""
done
if [[ $failures ]]; then
  echo "Test failed"
  echo "Failed tests:$failures"
  echo 'use "bash -i < $failed_test_name" to view the failure'
  exit 1
fi
exit 0
