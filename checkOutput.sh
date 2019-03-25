#!/bin/bash
sessionIdsFile=./Performance2/sessionIds
outputFile=./Performance2/log.json

missingIds=false

while read sessionId; do
  if ! grep -q "$sessionId" $outputFile; then
    missingIds=true
    break;
  fi
done < $sessionIdsFile

if "$missingIds"; then
    echo "There are missing IDs in $outputFile"
else
    echo "All IDs have been found in $outputFile"
fi
