#!/usr/bin/env bash
#
# Manage Android emulators for testing.
#
# Multiple emulators can coexist — each is identified by its AVD name.
#
# Usage:
#   ./emulator.sh start                  # start default AVD (test-android26-arm64)
#   ./emulator.sh start -f               # no-op if that AVD is already running
#   ./emulator.sh start --api 30         # start API 30 AVD
#   ./emulator.sh start --avd my-emu     # start a custom-named AVD
#   ./emulator.sh stop                   # stop default AVD
#   ./emulator.sh stop --avd my-emu      # stop specific AVD
#   ./emulator.sh restart                # stop + start default AVD
#   ./emulator.sh status                 # list all running emulators

set -euo pipefail

ANDROID_SDK="${ANDROID_HOME:-${HOME}/Library/Android/sdk}"
ADB="${ANDROID_SDK}/platform-tools/adb"
EMULATOR_BIN="${ANDROID_SDK}/emulator/emulator"
SDKMANAGER="${ANDROID_SDK}/cmdline-tools/latest/bin/sdkmanager"
AVDMANAGER="${ANDROID_SDK}/cmdline-tools/latest/bin/avdmanager"

# ── Helpers ───────────────────────────────────────────────────────────

# List running emulator serials (one per line)
running_emulators() {
    "$ADB" devices 2>/dev/null | grep "^emulator-" | awk '{print $1}' || true
}

# Find the serial of a running emulator by AVD name.
# Returns the serial on stdout, exit 0 if found, 1 if not.
find_serial_for_avd() {
    local target_avd="$1"
    local serials
    serials=$(running_emulators)
    [[ -z "$serials" ]] && return 1

    while read -r s; do
        [[ -z "$s" ]] && continue
        local avd
        avd=$("$ADB" -s "$s" emu avd name </dev/null 2>/dev/null | head -1 | tr -d '\r') || continue
        if [[ "$avd" == "$target_avd" ]]; then
            echo "$s"
            return 0
        fi
    done <<< "$serials"
    return 1
}

# Parse --api, --avd, -f from arguments.  Sets: api_level, avd_name, force
parse_args() {
    api_level=26
    avd_name=""
    force=false

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --api) api_level="$2"; shift 2 ;;
            --avd) avd_name="$2"; shift 2 ;;
            -f)    force=true; shift ;;
            *)     echo "Unknown option: $1"; exit 1 ;;
        esac
    done

    # Default AVD name derived from API level
    if [[ -z "$avd_name" ]]; then
        avd_name="test-android${api_level}-arm64"
    fi
}

do_status() {
    local devices
    devices=$("$ADB" devices 2>/dev/null | grep -v "^$" | grep -v "^List" | awk '{print $1}')

    if [[ -z "$devices" ]]; then
        echo "No connected devices."
        return
    fi

    printf "%-20s  %-10s  %-20s  %-6s  %s\n" "SERIAL" "TYPE" "AVD" "API" "ABIs"
    while read -r s; do
        local api abi type avd
        api=$("$ADB" -s "$s" shell getprop ro.build.version.sdk </dev/null 2>/dev/null | tr -d '\r') || api="?"
        abi=$("$ADB" -s "$s" shell getprop ro.product.cpu.abilist </dev/null 2>/dev/null | tr -d '\r') || abi="?"
        if [[ "$s" == emulator-* ]]; then
            type="emulator"
            avd=$("$ADB" -s "$s" emu avd name </dev/null 2>/dev/null | head -1 | tr -d '\r') || avd="?"
        else
            type="device"
            avd="-"
        fi
        printf "%-20s  %-10s  %-20s  %-6s  %s\n" "$s" "$type" "$avd" "$api" "$abi"
    done <<< "$devices"
}

do_stop() {
    parse_args "$@"

    local serial
    if serial=$(find_serial_for_avd "$avd_name"); then
        echo "=== Stopping emulator: ${serial} (AVD: ${avd_name}) ==="
        "$ADB" -s "$serial" emu kill 2>/dev/null || true
        for i in $(seq 1 30); do
            if ! find_serial_for_avd "$avd_name" >/dev/null 2>&1; then
                break
            fi
            sleep 1
        done
        echo "Emulator stopped."
    else
        echo "No running emulator for AVD '${avd_name}'."
    fi
}

do_start() {
    parse_args "$@"

    local system_image="system-images;android-${api_level};default;arm64-v8a"

    # Check if this AVD is already running
    local existing
    if existing=$(find_serial_for_avd "$avd_name"); then
        if [[ "$force" == true ]]; then
            echo "AVD '${avd_name}' already running: ${existing}"
            return 0
        else
            echo "ERROR: AVD '${avd_name}' already running: ${existing}"
            echo "  Use -f to skip, or stop it first: ./emulator.sh stop --avd ${avd_name}"
            exit 1
        fi
    fi

    # Ensure system image is installed
    if [[ ! -d "${ANDROID_SDK}/system-images/android-${api_level}/default/arm64-v8a" ]]; then
        echo "=== Installing system image: ${system_image} ==="
        echo "y" | "$SDKMANAGER" "$system_image"
    fi

    # Ensure AVD exists
    if ! "$EMULATOR_BIN" -list-avds 2>/dev/null | grep -q "^${avd_name}$"; then
        echo "=== Creating AVD: ${avd_name} ==="
        "$AVDMANAGER" create avd -n "$avd_name" -k "$system_image" -f <<< "no"
    fi

    # Remember which emulators are already running before we start
    local before
    before=$(running_emulators)

    # Start
    echo "=== Starting emulator: ${avd_name} ==="
    local logfile="emulator-$avd_name.log"
    "$EMULATOR_BIN" -avd "$avd_name" -no-window -no-audio -no-boot-anim \
        -gpu swiftshader_indirect &>"$logfile" &
    local pid=$!

    echo "Waiting for emulator to boot (PID: ${pid})..."

    # Wait for the new serial to appear
    local serial=""
    for i in $(seq 1 60); do
        serial=$(find_serial_for_avd "$avd_name" 2>/dev/null) && break
        sleep 2
    done

    if [[ -z "$serial" ]]; then
        echo "ERROR: Emulator process started but serial never appeared"
        exit 1
    fi

    echo "Emulator serial: ${serial}"

    # Wait for boot to complete on this specific serial
    for i in $(seq 1 120); do
        if "$ADB" -s "$serial" shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; then
            echo "Emulator booted: ${serial} (AVD: ${avd_name})"
            return 0
        fi
        sleep 2
    done

    echo "ERROR: Emulator failed to boot within 240 seconds"
    exit 1
}

# ── Main ──────────────────────────────────────────────────────────────
COMMAND="${1:-}"
shift || true

case "$COMMAND" in
    start)
        do_start "$@"
        ;;
    status)
        do_status
        ;;
    stop)
        do_stop "$@"
        ;;
    restart)
        do_stop "$@" 2>/dev/null || true
        do_start "$@"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status} [options]"
        echo ""
        echo "Options:"
        echo "  --avd NAME         AVD name (default: test-android26-arm64)"
        echo "  --api N            API level; sets AVD name to test-androidN-arm64 (default: 26)"
        echo "  -f                 No-op if AVD is already running (start only)"
        echo ""
        echo "Examples:"
        echo "  $0 start                  # start default AVD"
        echo "  $0 start --avd my-emu     # start custom AVD"
        echo "  $0 stop --avd my-emu      # stop only that AVD"
        echo "  $0 status                 # list all running emulators"
        exit 1
        ;;
esac
