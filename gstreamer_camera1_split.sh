#!/bin/bash

echo "📹 Camera1 (/dev/video0) 分岐開始"
echo "  → WebRTC: 直接 /dev/video0 から取得"
echo "  → v4l2loopback: /dev/video4 に出力"

# Camera1からv4l2loopbackへの分岐
gst-launch-1.0 \
  v4l2src device=/dev/video0 ! \
  image/jpeg,width=1280,height=720,framerate=30/1 ! \
  tee name=t1 \
  t1. ! queue ! v4l2sink device=/dev/video4 \
  t1. ! queue ! fakesink