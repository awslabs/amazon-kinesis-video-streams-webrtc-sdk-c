#!/bin/bash

# Set bash to print out every command that's running to the screen
set -xv

pwd
ls -l

pids=""
cd build
./kvsWebrtcClientMaster SampleChannel &
pids+=" $!"

sleep 2
./kvsWebrtcClientViewer SampleChannel &
pids+=" $!"

bash -c "sleep 10 && kill -s INT $(jobs -p)" &

for pid in $pids; do
  if ! wait $pid; then
    exit 1
  fi
done
