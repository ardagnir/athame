#!/bin/bash
text=$(cat $2)
for (( i=0; i<${#text}; i++ )); do
  if [ "${text:$i:1}" == "" ]; then
    # Type arrows without pauses
    echo -n "${text:$i:3}"
    i=$((i+2))
  else
    echo -n "${text:$i:1}"
  fi
  sleep $1
done
echo ""
