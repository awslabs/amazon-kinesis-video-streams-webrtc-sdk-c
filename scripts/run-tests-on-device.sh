#!/system/bin/sh
#
# Test runner script executed on the Android device/emulator.
# Pushed and invoked by test-android.sh.
#
# Usage: ./run-tests-on-device.sh [gtest_filter]

DEVICE_DIR="/data/local/tmp"
GTEST_FILTER="${1:-*}"

cd "${DEVICE_DIR}/tst"
export LD_LIBRARY_PATH="${DEVICE_DIR}"
export AWS_KVS_LOG_LEVEL="${AWS_KVS_LOG_LEVEL}"
export ASAN_OPTIONS="${ASAN_OPTIONS}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS}"

echo "=== Test run started: $(date) ==="
echo "Filter: ${GTEST_FILTER}"
echo ""

timeout 300 ./webrtc_client_test \
    --gtest_filter="${GTEST_FILTER}" \
    --gtest_break_on_failure 2>&1

EXIT_CODE=$?
echo ""
echo "=== Test run finished: $(date), exit code: ${EXIT_CODE} ==="
exit $EXIT_CODE
