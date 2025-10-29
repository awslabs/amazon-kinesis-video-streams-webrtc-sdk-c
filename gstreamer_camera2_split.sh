#!/bin/bash

echo "📹 Camera2 (/dev/video2) 分岐開始"
echo "  → WebRTC: 直接 /dev/video2 から取得"
echo "  → v4l2loopback: /dev/video5 に出力"

# Camera2からv4l2loopbackへの分岐
gst-launch-1.0 \
  v4l2src device=/dev/video2 ! \
  image/jpeg,width=1280,height=720,framerate=30/1 ! \
  tee name=t2 \
  t2. ! queue ! v4l2sink device=/dev/video5 \
  t2. ! queue ! fakesink