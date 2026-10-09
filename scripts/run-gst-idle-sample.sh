#!/bin/bash
#
# Runs the GStreamer testsrc master against a sequence of viewers and validates the
# idle behaviour of the sender pipeline in samples/common/GstMedia.c:
#   1. The pipeline is stopped once the last viewer disconnects
#      (log: "No viewers connected, stopping the media pipeline") and the master keeps running
#   2. The pipeline is restarted for the next viewer, which receives decodable video
#   3. The pipeline keeps running while at least one viewer is still connected
#   4. The master exits cleanly on SIGINT while the pipeline is stopped (zero viewers)
#   5. The master exits cleanly on SIGINT when no viewer ever connected
#
# Usage: ./run-gst-idle-sample.sh <channel-name>
#
# Environment:
#   KVS_ICE_TRANSPORT_POLICY - ICE policy for master and viewers (not asserted on by this script)
#   AWS_KVS_LOG_LEVEL        - must be 1 (VERBOSE): the viewer frame logs this script relies on
#                              are VERBOSE and the pipeline state logs are DEBUG. Defaults to 1.
#
# Each viewer runs in its own sub-directory because kvsWebrtcClientViewerGstSample writes
# video.mkv relative to the working directory.

set -uo pipefail

CHANNEL_NAME="${1:?Usage: $0 <channel-name>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SAMPLES_DIR="$SCRIPT_DIR/../build/samples"
MASTER_BIN="$SAMPLES_DIR/kvsWebrtcClientMasterGstSample"
VIEWER_BIN="$SAMPLES_DIR/kvsWebrtcClientViewerGstSample"

# Log lines emitted by the sample that this script keys on
PIPELINE_STARTED="null->playing"
PIPELINE_STOPPED="No viewers connected, stopping the media pipeline"
SESSION_FREED="Freeing streaming session"
MASTER_READY="set up done"
THREAD_EXITED="Media sender thread exited gracefully"
MASTER_ERRORS="Received error from GStreamer|State change to PLAYING failed|State change to NULL failed|Pipeline error"

# Timeouts (seconds)
READY_TIMEOUT=60      # master signaling setup
FRAMES_TIMEOUT=90     # viewer connect + first frames
STOP_TIMEOUT=90       # viewer disconnect -> pipeline stopped (ICE disconnect detection is up to ~30 s)
SHUTDOWN_TIMEOUT=15   # SIGINT -> process exit
SETTLE_SECONDS=5      # > GST_PIPELINE_POLL_INTERVAL_MS plus session-free lock wait
MIN_FRAMES=25         # ~1 s of video at 25 fps

LOG_LEVEL="${AWS_KVS_LOG_LEVEL:-1}"
if [ "$LOG_LEVEL" -gt 1 ]; then
    echo "FAILURE: AWS_KVS_LOG_LEVEL must be 1 (VERBOSE) for this script, got $LOG_LEVEL"
    exit 1
fi
export AWS_KVS_LOG_LEVEL="$LOG_LEVEL"

# The sample logger is unflushed stdio; when stdout is a file the last 4 KB only appear on the
# next write or at exit. Line-buffer it so the log can be asserted on while the process runs.
STDBUF=""
if command -v stdbuf > /dev/null 2>&1; then
    STDBUF="stdbuf -oL"
elif command -v gstdbuf > /dev/null 2>&1; then
    STDBUF="gstdbuf -oL" # Homebrew coreutils on macOS
else
    echo "WARNING: stdbuf not found, log assertions may lag behind the process"
fi

cd "$SAMPLES_DIR" || exit 1
rm -f .SignalingCache_* master.log master-no-viewer.log
rm -rf viewer-1 viewer-2 viewer-3

MASTER_PID=""
SOFT_FAILURES=0

log() {
    echo "[$(date -u +%H:%M:%S)] $*"
}

fail() {
    echo "FAILURE: $*"
    exit 1
}

soft_fail() {
    echo "WARNING: $*"
    SOFT_FAILURES=$((SOFT_FAILURES + 1))
}

dump_logs() {
    echo "=== Master Log ==="
    cat master.log 2>/dev/null || echo "(no master.log)"
    echo "=== Master Log (no viewer run) ==="
    cat master-no-viewer.log 2>/dev/null || echo "(no master-no-viewer.log)"
    for v in viewer-1 viewer-2 viewer-3; do
        echo "=== $v Log (tail) ==="
        tail -n 100 "$v/viewer.log" 2>/dev/null || echo "(no $v/viewer.log)"
    done
}

cleanup() {
    local rc=$?
    for pid in $MASTER_PID $VIEWER_PIDS; do
        kill -9 "$pid" 2>/dev/null
    done
    if [ $rc -ne 0 ]; then
        dump_logs
    fi
}
VIEWER_PIDS=""
trap cleanup EXIT

is_alive() {
    kill -0 "$1" 2>/dev/null
}

# count_in <file> <pattern>
count_in() {
    local n
    n=$(grep -c -- "$2" "$1" 2>/dev/null)
    echo "${n:-0}"
}

# wait_for_count <file> <pattern> <min-count> <timeout-seconds>
wait_for_count() {
    local file=$1 pattern=$2 want=$3 timeout=$4 waited=0
    while [ "$waited" -lt "$timeout" ]; do
        if [ "$(count_in "$file" "$pattern")" -ge "$want" ]; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    return 1
}

# stop_process <pid> <name>: SIGINT, wait up to SHUTDOWN_TIMEOUT, SIGKILL fallback. Returns the exit code.
stop_process() {
    local pid=$1 name=$2 waited=0
    kill -s INT "$pid" 2>/dev/null
    while is_alive "$pid" && [ "$waited" -lt "$SHUTDOWN_TIMEOUT" ]; do
        sleep 1
        waited=$((waited + 1))
    done
    if is_alive "$pid"; then
        echo "WARNING: $name did not exit within ${SHUTDOWN_TIMEOUT}s after SIGINT, killing it"
        kill -9 "$pid" 2>/dev/null
    fi
    wait "$pid" 2>/dev/null
    return $?
}

# start_master <log-file>: sets MASTER_PID
start_master() {
    local logfile=$1
    $STDBUF "$MASTER_BIN" "$CHANNEL_NAME" video-only testsrc > "$logfile" 2>&1 &
    MASTER_PID=$!
    wait_for_count "$logfile" "$MASTER_READY" 1 "$READY_TIMEOUT" ||
        fail "master did not finish signaling setup within ${READY_TIMEOUT}s"
    log "master started (pid $MASTER_PID)"
}

# start_viewer <name>: runs the viewer in ./<name>/, sets VIEWER_PID, waits for first frames
start_viewer() {
    local name=$1
    mkdir -p "$name"
    (cd "$name" && exec $STDBUF "$VIEWER_BIN" "$CHANNEL_NAME" video-only) > "$name/viewer.log" 2>&1 &
    VIEWER_PID=$!
    VIEWER_PIDS="$VIEWER_PIDS $VIEWER_PID"
    wait_for_count "$name/viewer.log" "Video frame size" "$MIN_FRAMES" "$FRAMES_TIMEOUT" ||
        fail "$name did not receive $MIN_FRAMES video frames within ${FRAMES_TIMEOUT}s"
    log "$name connected and receiving video (pid $VIEWER_PID)"
}

# stop_viewer <pid> <name>: a viewer that does not shut down cleanly is a soft failure; the
# master-side assertions still run (a killed viewer is just a harsher disconnect for the master)
UNCLEAN_VIEWERS=""
stop_viewer() {
    local pid=$1 name=$2 rc
    stop_process "$pid" "$name"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        soft_fail "$name exit code $rc"
        UNCLEAN_VIEWERS="$UNCLEAN_VIEWERS $name"
    else
        log "$name exited cleanly"
    fi
}

# diagnose_viewer <name>: printed when a viewer's capture is not decodable. The usual cause is that the viewer never
# received a key frame: the one emitted when the pipeline (re)started was dropped because the viewer's SRTP session was
# not ready yet, or the viewer joined a running pipeline, and the sample waited for the encoder's next periodic key frame.
diagnose_viewer() {
    local name=$1 frames max_size
    echo "--- diagnostics for $name ---"
    echo "master: pipeline starts=$(count_in master.log "$PIPELINE_STARTED") stops=$(count_in master.log "$PIPELINE_STOPPED")" \
        "frames dropped before SRTP ready=$(count_in master.log "SRTP not ready yet")" \
        "key frame requests=$(count_in master.log "Requesting a key frame")"
    echo "master timeline:"
    grep -E -- "$PIPELINE_STARTED|$PIPELINE_STOPPED|SRTP not ready yet|Time to first frame|Requesting a key frame|$SESSION_FREED" master.log |
        cut -c1-160 | tail -n 40
    frames=$(count_in "$name/viewer.log" "Video frame size")
    max_size=$(grep -o 'Video frame size: [0-9]*' "$name/viewer.log" 2>/dev/null | awk '{ if ($4 > m) m = $4 } END { print m + 0 }')
    echo "$name: $frames video frames received, largest frame $max_size bytes" \
        "(a 720p key frame is tens of KB; if every frame is small the viewer only received delta frames)"
    echo "$name errors:"
    grep -E 'ERROR|Error received' "$name/viewer.log" | cut -c1-160 | tail -n 5
    echo "--- end diagnostics for $name ---"
}

# check_decodable <name>: skipped for a viewer that did not exit cleanly (its mkv is not finalized)
check_decodable() {
    local name=$1
    case " $UNCLEAN_VIEWERS " in
        *" $name "*)
            echo "WARNING: skipping decode check for $name (did not exit cleanly)"
            return 0
            ;;
    esac
    if [ ! -s "$name/video.mkv" ]; then
        diagnose_viewer "$name"
        fail "$name/video.mkv was not generated or is empty"
    fi
    if gst-launch-1.0 filesrc location="$name/video.mkv" ! matroskademux ! decodebin ! fakesink > "$name/decode.log" 2>&1; then
        echo "SUCCESS: $name/video.mkv decoded successfully ($(stat -c%s "$name/video.mkv" 2>/dev/null || stat -f%z "$name/video.mkv") bytes)"
    else
        cat "$name/decode.log"
        diagnose_viewer "$name"
        fail "$name/video.mkv could not be decoded"
    fi
}

assert_master_alive() {
    is_alive "$MASTER_PID" || fail "master exited unexpectedly ($1)"
}

###############################################################################
# 1. Pipeline stops after the last viewer disconnects
###############################################################################
log "Scenario 1: pipeline stops after the last viewer disconnects"
start_master master.log
start_viewer viewer-1
VIEWER_1_PID=$VIEWER_PID

wait_for_count master.log "$PIPELINE_STARTED" 1 10 || fail "pipeline did not start for the first viewer"
[ "$(count_in master.log "$PIPELINE_STOPPED")" -eq 0 ] || fail "pipeline was stopped while a viewer was connected"

stop_viewer "$VIEWER_1_PID" viewer-1
wait_for_count master.log "$PIPELINE_STOPPED" 1 "$STOP_TIMEOUT" ||
    fail "pipeline was not stopped within ${STOP_TIMEOUT}s after the last viewer disconnected"
assert_master_alive "after the last viewer disconnected"
echo "SUCCESS: pipeline stopped after the last viewer disconnected, master still running"

###############################################################################
# 2. Pipeline restarts for the next viewer
###############################################################################
log "Scenario 2: pipeline restarts for the next viewer"
start_viewer viewer-2
VIEWER_2_PID=$VIEWER_PID

[ "$(count_in master.log "$PIPELINE_STARTED")" -ge 2 ] || fail "pipeline was not restarted for the next viewer"
echo "SUCCESS: pipeline restarted and the new viewer receives video"

###############################################################################
# 3. Pipeline keeps running while another viewer is still connected
###############################################################################
log "Scenario 3: pipeline keeps running while another viewer is still connected"
start_viewer viewer-3
VIEWER_3_PID=$VIEWER_PID

STOPS_BEFORE=$(count_in master.log "$PIPELINE_STOPPED")
FREED_BEFORE=$(count_in master.log "$SESSION_FREED")
stop_viewer "$VIEWER_2_PID" viewer-2
wait_for_count master.log "$SESSION_FREED" $((FREED_BEFORE + 1)) "$STOP_TIMEOUT" ||
    fail "master did not free the departed viewer's session within ${STOP_TIMEOUT}s"
FRAMES_BEFORE=$(count_in viewer-3/viewer.log "Video frame size")
sleep "$SETTLE_SECONDS"
STOPS_AFTER=$(count_in master.log "$PIPELINE_STOPPED")
FRAMES_AFTER=$(count_in viewer-3/viewer.log "Video frame size")

[ "$STOPS_AFTER" -eq "$STOPS_BEFORE" ] || fail "pipeline was stopped while viewer-3 was still connected"
[ "$FRAMES_AFTER" -gt "$FRAMES_BEFORE" ] || fail "viewer-3 stopped receiving video after viewer-2 left ($FRAMES_BEFORE -> $FRAMES_AFTER frames)"
assert_master_alive "after one of two viewers disconnected"
echo "SUCCESS: pipeline kept running for the remaining viewer ($FRAMES_BEFORE -> $FRAMES_AFTER frames)"

stop_viewer "$VIEWER_3_PID" viewer-3
wait_for_count master.log "$PIPELINE_STOPPED" $((STOPS_BEFORE + 1)) "$STOP_TIMEOUT" ||
    fail "pipeline was not stopped within ${STOP_TIMEOUT}s after the last viewer disconnected (second cycle)"
assert_master_alive "after the second stop"
echo "SUCCESS: pipeline stopped again after the last viewer disconnected"

###############################################################################
# 4. Master exits cleanly on SIGINT with the pipeline stopped
###############################################################################
log "Scenario 4: master exits cleanly on SIGINT with zero viewers"
stop_process "$MASTER_PID" master
MASTER_EXIT=$?
[ "$MASTER_EXIT" -eq 0 ] || fail "master exit code $MASTER_EXIT on SIGINT with the pipeline stopped"
grep -q -- "$THREAD_EXITED" master.log || fail "media sender thread did not report a graceful exit"
MASTER_PID=""
echo "SUCCESS: master exited cleanly with the pipeline stopped"

# Whole-run checks on the master log
STARTS=$(count_in master.log "$PIPELINE_STARTED")
STOPS=$(count_in master.log "$PIPELINE_STOPPED")
[ "$STARTS" -eq 2 ] || fail "expected the pipeline to start exactly twice, saw $STARTS"
[ "$STOPS" -eq 2 ] || fail "expected the pipeline to stop exactly twice, saw $STOPS"
if grep -E -- "$MASTER_ERRORS" master.log; then
    fail "master log contains GStreamer/state-change errors"
fi
echo "SUCCESS: pipeline started $STARTS times and stopped $STOPS times with no GStreamer errors"

for v in viewer-1 viewer-2 viewer-3; do
    check_decodable "$v"
done

###############################################################################
# 5. Master exits cleanly on SIGINT when no viewer ever connected
###############################################################################
log "Scenario 5: master exits cleanly on SIGINT when no viewer ever connected"
rm -f .SignalingCache_*
start_master master-no-viewer.log
sleep 5
stop_process "$MASTER_PID" master
MASTER_EXIT=$?
[ "$MASTER_EXIT" -eq 0 ] || fail "master exit code $MASTER_EXIT on SIGINT with no viewer ever connected"
MASTER_PID=""
echo "SUCCESS: master exited cleanly with no viewer ever connected"

if [ "$SOFT_FAILURES" -ne 0 ]; then
    fail "$SOFT_FAILURES viewer(s) did not shut down cleanly (see WARNING lines above); all master-side checks passed"
fi

echo "SUCCESS: GStreamer idle pipeline sample completed on channel $CHANNEL_NAME"
