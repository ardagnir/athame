#!/bin/bash
text=$(cat $2)
for (( i=0; i<${#text}; i++ )); do
  echo -n "${text:$i:1}"
  sleep $1
done
echo ""
