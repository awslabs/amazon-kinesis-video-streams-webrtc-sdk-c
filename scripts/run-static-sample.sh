#!/bin/bash
#
# Runs the static frames master+viewer sample and validates the connection.
#
# Usage: ./run-static-sample.sh <channel-name>
#
# Environment:
#   KVS_ICE_TRANSPORT_POLICY - set to "relay" to force TURN and verify relay candidates
#   AWS_KVS_LOG_LEVEL        - log verbosity (default: inherited from environment)

set -uo pipefail

CHANNEL_NAME="${1:?Usage: $0 <channel-name>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SAMPLES_DIR="$SCRIPT_DIR/../build/samples"

cd "$SAMPLES_DIR"
rm -f .SignalingCache_*

dump_logs() {
    echo "=== Master Log ==="
    cat master.log 2>/dev/null || echo "(no master.log)"
    echo "=== Viewer Log ==="
    cat viewer.log 2>/dev/null || echo "(no viewer.log)"
}
trap 'if [ $? -ne 0 ]; then dump_logs; fi' EXIT

"./kvsWebrtcClientMaster" "$CHANNEL_NAME" > master.log 2>&1 &
MASTER_PID=$!
sleep 15
"./kvsWebrtcClientViewer" "$CHANNEL_NAME" > viewer.log 2>&1 &
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

if [ "${KVS_ICE_TRANSPORT_POLICY:-}" = "relay" ]; then
    if grep "local candidate type: relay. remote candidate type: relay" master.log || \
       grep "local candidate type: relay. remote candidate type: relay" viewer.log; then
        echo "SUCCESS: Force TURN verified - relay candidates selected"
    else
        echo "FAILURE: Expected relay candidate pair not found in logs"
        cat master.log
        cat viewer.log
        exit 1
    fi
else
    if grep "local candidate type:.*remote candidate type:" master.log || \
       grep "local candidate type:.*remote candidate type:" viewer.log; then
        echo "SUCCESS: ICE candidate pair selected"
    else
        echo "FAILURE: No ICE candidate pair selection found in logs"
        cat master.log
        cat viewer.log
        exit 1
    fi
fi

echo "SUCCESS: Static frames sample completed on channel $CHANNEL_NAME"
