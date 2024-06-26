#!/bin/bash

if [[ -z "$AWS_ACCESS_KEY_ID" || -z "$AWS_SECRET_ACCESS_KEY" || -z "$AWS_SESSION_TOKEN" ]]
then
  echo "Couldn't find AWS credentials. Very likely this build is coming from a fork. Ignoring."
  exit 0
fi

# Set bash to print out every command that's running to the screen
# Set logging after checking credentials so that we don't leak them
set -xv

pwd
ls -l

pids=""
cd build/samples
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
