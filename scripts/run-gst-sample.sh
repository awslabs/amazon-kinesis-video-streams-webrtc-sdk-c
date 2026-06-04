#!/bin/bash
#
# Runs the GStreamer testsrc master+viewer sample and validates:
#   - Connection succeeds
#   - Verifies relay candidates when KVS_ICE_TRANSPORT_POLICY=relay
#   - Viewer receives video frames
#   - video.mkv is generated and non-empty
#   - video.mkv can be decoded without errors
#
# Usage: ./run-gst-sample.sh <channel-name>
#
# Environment:
#   KVS_ICE_TRANSPORT_POLICY - set to "relay" to force TURN and verify relay candidates
#   AWS_KVS_LOG_LEVEL        - log verbosity (use 1 for verbose to see frame logs)

set -uo pipefail

CHANNEL_NAME="${1:?Usage: $0 <channel-name>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SAMPLES_DIR="$SCRIPT_DIR/../build/samples"

cd "$SAMPLES_DIR"
rm -f video.mkv .SignalingCache_*

dump_logs() {
    echo "=== Master Log ==="
    cat master.log 2>/dev/null || echo "(no master.log)"
    echo "=== Viewer Log ==="
    cat viewer.log 2>/dev/null || echo "(no viewer.log)"
}
trap 'if [ $? -ne 0 ]; then dump_logs; fi' EXIT

"./kvsWebrtcClientMasterGstSample" "$CHANNEL_NAME" video-only testsrc > master.log 2>&1 &
MASTER_PID=$!
sleep 15
"./kvsWebrtcClientViewerGstSample" "$CHANNEL_NAME" video-only > viewer.log 2>&1 &
VIEWER_PID=$!

# Let them run for 60s, then send SIGINT
sleep 60
kill -s INT $MASTER_PID $VIEWER_PID 2>/dev/null

# Give them 15s to shut down gracefully, then force kill
(sleep 15 && kill -9 $MASTER_PID $VIEWER_PID 2>/dev/null) &
WATCHDOG_PID=$!

wait $MASTER_PID 2>/dev/null
MASTER_EXIT=$?
wait $VIEWER_PID 2>/dev/null
VIEWER_EXIT=$?

kill $WATCHDOG_PID 2>/dev/null
wait $WATCHDOG_PID 2>/dev/null

if [ $MASTER_EXIT -ne 0 ] || [ $VIEWER_EXIT -ne 0 ]; then
    echo "Sample execution failed. Master exit code: $MASTER_EXIT, Viewer exit code: $VIEWER_EXIT"
    exit 1
fi

# Verify ICE candidate pair selection
if [ "${KVS_ICE_TRANSPORT_POLICY:-}" = "relay" ]; then
    if grep "local candidate type: relay. remote candidate type: relay" master.log || \
       grep "local candidate type: relay. remote candidate type: relay" viewer.log; then
        echo "SUCCESS: Force TURN verified - relay candidates selected"
    else
        echo "FAILURE: Expected relay candidate pair not found in logs"
        exit 1
    fi
else
    if grep "local candidate type:.*remote candidate type:" master.log || \
       grep "local candidate type:.*remote candidate type:" viewer.log; then
        echo "SUCCESS: ICE candidate pair selected"
    else
        echo "FAILURE: No ICE candidate pair selection found in logs"
        exit 1
    fi
fi

# Validate frames were received by the viewer
if grep -q "Video frame size" viewer.log; then
    FRAME_COUNT=$(grep -c "Video frame size" viewer.log)
    echo "SUCCESS: Viewer received $FRAME_COUNT video frames"
else
    echo "FAILURE: Viewer did not receive any video frames"
    exit 1
fi

# Validate the generated video.mkv file exists and is non-empty
if [ -s video.mkv ]; then
    FILE_SIZE=$(stat -c%s video.mkv 2>/dev/null || stat -f%z video.mkv)
    echo "SUCCESS: video.mkv generated ($FILE_SIZE bytes)"
else
    echo "FAILURE: video.mkv was not generated or is empty"
    ls -la *.mkv 2>/dev/null || echo "No .mkv files found"
    exit 1
fi

# Validate the file is not corrupted by decoding it
if gst-launch-1.0 filesrc location=video.mkv ! matroskademux ! decodebin ! fakesink 2>&1 | tee decode.log; then
    echo "SUCCESS: video.mkv decoded successfully"
else
    echo "FAILURE: video.mkv could not be decoded"
    cat decode.log
    exit 1
fi

echo "SUCCESS: GStreamer testsrc sample completed on channel $CHANNEL_NAME"
