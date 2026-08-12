#!/bin/bash
# Generate 4 sets of H264 (or H265) sample frames with different videotestsrc patterns
# and a large centered "Track #N" text overlay for multi-track demo.
#
# Requires: gstreamer, gst-plugins-base, gst-plugins-good, gst-plugins-ugly (x264enc),
#           gst-plugins-bad (x265enc, for some patterns), and the pango text overlay plugin.
#
# The multi-track sample uses a relative file path for the frames. It searches current directory.
#
# Usage: Run from the build/ directory:
#   cd build && ../scripts/generate_multi_track_frames.sh [h264|h265]
#   ./samples/kvsWebrtcClientMaster4Video1Audio <channelName> [h264|h265]
#
# Usage: Run from the build/samples/ directory:
#   cd build/samples/ && ../../scripts/generate_multi_track_frames.sh [h264|h265]
#   ./kvsWebrtcClientMaster4Video1Audio <channelName> [h264|h265]
#
# The optional codec argument (default h264) controls which encoder is used and
# which per-track directories (h264SampleFrames_trackN / h265SampleFrames_trackN)
# are produced. It must match the codec passed to kvsWebrtcClientMaster4Video1Audio.

set -e

CODEC="${1:-h264}"

PATTERNS=("ball" "smpte" "snow" "circular")
NUM_BUFFERS=1500
WIDTH=1280
HEIGHT=720
FPS=25
BITRATE=512

case "${CODEC}" in
    h264)
        DIR_PREFIX="h264SampleFrames"
        EXT="h264"
        ENCODER="x264enc bframes=0 speed-preset=veryfast bitrate=${BITRATE} byte-stream=TRUE tune=zerolatency key-int-max=25"
        # config-interval=-1 repeats SPS/PPS before every keyframe so each keyframe
        # is a self-contained access unit the WebRTC decoder can recover from.
        PARSER="h264parse config-interval=-1"
        CAPS="video/x-h264,stream-format=byte-stream,alignment=au,profile=baseline"
        ;;
    h265)
        DIR_PREFIX="h265SampleFrames"
        EXT="h265"
        ENCODER="x265enc speed-preset=veryfast tune=zerolatency key-int-max=25 bitrate=${BITRATE} option-string=bframes=0"
        # config-interval=-1 repeats VPS/SPS/PPS before every keyframe so each keyframe
        # is a self-contained access unit the WebRTC decoder can recover from.
        PARSER="h265parse config-interval=-1"
        CAPS="video/x-h265,stream-format=byte-stream,alignment=au"
        ;;
    *)
        echo "Unsupported codec '${CODEC}'. Use 'h264' or 'h265'."
        exit 1
        ;;
esac

echo "Generating ${CODEC} frames..."
echo "Working directory: $(pwd)"
echo "Frames will be written under: $(pwd)/${DIR_PREFIX}_track{0..3}/"
echo "The sample resolves these paths relative to its own working directory;"
echo "run it from here (or wherever ./${DIR_PREFIX}_track0/ is visible)."
echo ""

for i in 0 1 2 3; do
    DIR="${DIR_PREFIX}_track${i}"
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
        ${ENCODER} ! \
        ${PARSER} ! \
        ${CAPS} ! \
        multifilesink location="${DIR}/frame-%04d.${EXT}" index=1

    echo "Track ${i} done: $(ls ${DIR}/frame-*.${EXT} | wc -l) frames in $(cd "${DIR}" && pwd)/"
done

echo ""
echo "All 4 track frame sets generated successfully under $(pwd)/"
