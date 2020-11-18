#!/bin/bash

bins=(
  kvsWebrtcClientMaster
  kvsWebrtcClientViewer
  discoverNatBehavior
  libkvsWebrtcClient.a
  libkvsWebrtcSignalingClient.a
)

for bin in ${bins[@]}
do
  # Expect to only have a dynamic link to musl's libc
  if ldd ${bin} | grep -v musl &> /dev/null; then
    echo "${bin}: failed"
    echo ""
    echo "Found dynamic links:"
    ldd ${bin}
    exit 1
  else
    echo "${bin}: passed"
  fi
done
