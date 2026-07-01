#!/bin/bash
# Generate 4 sets of H264 sample frames with different videotestsrc patterns
# and a large centered "Track #N" text overlay for multi-track demo.
#
# Requires: gstreamer, gst-plugins-base, gst-plugins-good, gst-plugins-ugly (x264enc),
#           gst-plugins-bad (for some patterns), and the pango text overlay plugin.
#
# Usage: Run from the samples/ directory:
#   cd samples && bash generate_multi_track_frames.sh

set -e

PATTERNS=("ball" "smpte" "snow" "circular")
NUM_BUFFERS=1500
WIDTH=1280
HEIGHT=720
FPS=25
BITRATE=512

for i in 0 1 2 3; do
    DIR="h264SampleFrames_track${i}"
    PATTERN="${PATTERNS[$i]}"

    echo "Generating track ${i} frames with pattern '${PATTERN}' into ${DIR}/"
    mkdir -p "${DIR}"

    gst-launch-1.0 -e \
        videotestsrc pattern="${PATTERN}" num-buffers=${NUM_BUFFERS} ! \
        videoconvert ! \
        video/x-raw,format=I420,width=${WIDTH},height=${HEIGHT},framerate=${FPS}/1 ! \
        textoverlay text="Track #${i}" valignment=center halignment=center font-desc="Sans Bold 72" ! \
        videoconvert ! \
        video/x-raw,format=I420 ! \
        queue ! \
        x264enc bframes=0 speed-preset=veryfast bitrate=${BITRATE} byte-stream=TRUE tune=zerolatency key-int-max=25 ! \
        video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline ! \
        multifilesink location="${DIR}/frame-%04d.h264" index=1

    echo "Track ${i} done: $(ls ${DIR}/frame-*.h264 | wc -l) frames generated."
done

echo ""
echo "All 4 track frame sets generated successfully."
echo "Copy them to your build directory before running the sample."
