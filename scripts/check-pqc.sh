#!/bin/bash
#
# Strict gate: every TLS 1.3 handshake the SDK makes to a KVS host must
# negotiate X25519MLKEM768.
#
# Usage:
#   ./scripts/check-pqc.sh <channel-name> [build-dir]
#
# Default build-dir is "build". The build must have been configured with
# -DUSE_AWS_LC=ON. Requires AWS credentials, tcpdump and tshark, and the ability
# to run tcpdump (root, or CAP_NET_RAW).
#
# What this proves, and why it is done this way
# ---------------------------------------------
# It runs the real master and viewer samples and reads the handshakes off the
# network interface. Nothing in the SDK takes part. In TLS 1.3 the ServerHello
# is in cleartext and its key_share extension names the group the server
# selected, so the negotiated group is readable without decrypting anything and
# without any diagnostic code in the SDK.
#
# That is the whole point. Every other check we have is either a library
# self-report or a probe that links the same AWS-LC. This one observes the
# shipping sample doing its real work.
#
# All three TLS 1.3 client paths appear in one capture, as separate TCP streams:
#
#   control plane HTTPS  DescribeSignalingChannel, GetSignalingChannelEndpoint
#   data plane HTTPS     GetIceServerConfig
#   signaling WSS        ConnectAsMaster / ConnectAsViewer
#   TURN over TLS        turns:
#
# Streams are selected by SNI rather than by IP. The SDK's resolver can return a
# different address than the shell's, so an IP filter finds nothing.
#
# STRICT: a classical group on any KVS handshake fails the run. The service
# rolls ML-KEM out per endpoint, so pin this job to a schedule or a branch where
# a service-side change is a signal worth acting on, not a surprise on an
# unrelated pull request.

set -uo pipefail

CHANNEL_NAME="${1:?Usage: $0 <channel-name> [build-dir]}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR_ARG="${2:-build}"
BUILD_DIR="$(cd "$REPO_ROOT/$BUILD_DIR_ARG" 2>/dev/null && pwd)" || {
    echo "FAIL: build directory '$REPO_ROOT/$BUILD_DIR_ARG' not found. Build first:"
    echo "        mkdir build && cd build && cmake .. -DUSE_AWS_LC=ON && make -j"
    exit 1
}

# tshark prints supported_groups in hex and key_share in decimal. 0x11EC = 4588.
PQC_GROUP_NAME="X25519MLKEM768"
PQC_GROUP_DEC=4588
SNI_MATCH="kinesisvideo"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-45}"
# A capture with too few handshakes means the samples never got going. Passing on
# that would be a false green, so require at least the control-plane calls.
MIN_HANDSHAKES="${MIN_HANDSHAKES:-3}"

WORK="$(mktemp -d)"
PCAP="$WORK/kvs-pqc.pcapng"

echo "=============================================================="
echo " PQC handshake gate (live, strict)"
echo "=============================================================="
echo "build dir : $BUILD_DIR"
echo "channel   : $CHANNEL_NAME"
echo "capture   : ${CAPTURE_SECONDS}s"
echo "expecting : $PQC_GROUP_NAME ($PQC_GROUP_DEC) on every $SNI_MATCH handshake"
echo

for tool in tcpdump; do
    command -v "$tool" > /dev/null || {
        echo "FAIL: $tool not found on PATH"
        exit 1
    }
done

# On macOS the Wireshark CLI tools live inside the app bundle and are not on PATH.
TSHARK="$(command -v tshark || true)"
if [ -z "$TSHARK" ] && [ -x /Applications/Wireshark.app/Contents/MacOS/tshark ]; then
    TSHARK=/Applications/Wireshark.app/Contents/MacOS/tshark
fi
if [ -z "$TSHARK" ]; then
    echo "FAIL: tshark not found. Install Wireshark, or on Debian/Ubuntu: apt-get install tshark"
    exit 1
fi

# Capture interface and privilege. Opening a capture needs root or CAP_NET_RAW.
# Prefer a non-interactive sudo, which CI runners allow. Fall back to an
# unprivileged capture, which works on macOS once Wireshark's installer has put
# the user in the access_bpf group.
#
# tcpdump -D only lists interfaces. On a GitHub runner it succeeds while the
# capture itself is still refused with "Operation not permitted", so it cannot
# be used to decide whether sudo is needed.
#
# -i any is deliberately not the macOS default: there it needs root and captures
# nothing without it, so fall back to the default-route interface instead.
PQC_IFACE="${PQC_IFACE:-}"
if [ -z "$PQC_IFACE" ]; then
    if [ "$(uname -s)" = "Darwin" ]; then
        PQC_IFACE="$(route -n get default 2>/dev/null | awk '/interface:/{print $2}')"
        PQC_IFACE="${PQC_IFACE:-en0}"
    else
        PQC_IFACE="any"
    fi
fi

SUDO=""
if sudo -n true > /dev/null 2>&1; then
    SUDO="sudo"
elif ! tcpdump -D > /dev/null 2>&1; then
    echo "FAIL: cannot open a capture device, and sudo needs a password."
    echo "      Either run this with sudo, or on macOS join the access_bpf group"
    echo "      (the Wireshark installer offers this). Check with: id -Gn | grep access_bpf"
    exit 1
fi

TCPDUMP_PID=""
MASTER_PID=""
VIEWER_PID=""

cleanup() {
    [ -n "$VIEWER_PID" ] && kill -9 "$VIEWER_PID" 2> /dev/null
    [ -n "$MASTER_PID" ] && kill -9 "$MASTER_PID" 2> /dev/null
    [ -n "$TCPDUMP_PID" ] && $SUDO kill -INT "$TCPDUMP_PID" 2> /dev/null
    wait 2> /dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT

# --------------------------------------------------------------------------
echo "[1/3] Capturing while the samples run"
# --------------------------------------------------------------------------
echo "  interface: $PQC_IFACE${SUDO:+ (via sudo)}"
# The BPF filter runs in the kernel, so unrelated traffic never reaches the file.
$SUDO tcpdump -i "$PQC_IFACE" -s 0 -w "$PCAP" 'tcp port 443' > "$WORK/tcpdump.log" 2>&1 &
TCPDUMP_PID=$!
sleep 3
if ! kill -0 "$TCPDUMP_PID" 2> /dev/null; then
    echo "FAIL: tcpdump did not start"
    cat "$WORK/tcpdump.log"
    exit 1
fi

cd "$BUILD_DIR/samples" || {
    echo "FAIL: $BUILD_DIR/samples not found"
    exit 1
}

./kvsWebrtcClientMaster "$CHANNEL_NAME" > "$WORK/master.log" 2>&1 &
MASTER_PID=$!
sleep 10
./kvsWebrtcClientViewer "$CHANNEL_NAME" > "$WORK/viewer.log" 2>&1 &
VIEWER_PID=$!

sleep "$CAPTURE_SECONDS"

kill -s INT "$VIEWER_PID" 2> /dev/null
sleep 3
kill -s INT "$MASTER_PID" 2> /dev/null
sleep 5
kill -9 "$VIEWER_PID" "$MASTER_PID" 2> /dev/null
VIEWER_PID=""
MASTER_PID=""

# tcpdump buffers; SIGINT makes it flush and close the file.
$SUDO kill -INT "$TCPDUMP_PID" 2> /dev/null
TCPDUMP_PID=""
sleep 2
$SUDO chmod a+r "$PCAP" 2> /dev/null

if [ ! -s "$PCAP" ]; then
    echo "FAIL: no capture file was written"
    cat "$WORK/tcpdump.log"
    exit 1
fi
echo "  capture: $(du -h "$PCAP" | cut -f1)"

if grep -q "New connection state 3" "$WORK/master.log"; then
    echo "  samples: peer connection established"
else
    # Not fatal on its own. The handshakes that matter happen before ICE, and a
    # P2P failure on a CI runner is a different problem from a PQC regression.
    echo "  samples: peer connection NOT established (handshake checks still apply)"
fi
echo

# --------------------------------------------------------------------------
echo "[2/3] What the SDK offered"
# --------------------------------------------------------------------------
# ClientHellos whose SNI names a KVS host, one line per handshake.
"$TSHARK" -r "$PCAP" \
    -Y "tls.handshake.type == 1 && tls.handshake.extensions_server_name contains \"$SNI_MATCH\"" \
    -T fields -e tcp.stream -e tls.handshake.extensions_server_name \
    -e tls.handshake.extensions_supported_group > "$WORK/offers.tsv" 2> "$WORK/tshark.err"

offered=$(wc -l < "$WORK/offers.tsv" | tr -d ' ')
if [ "$offered" -eq 0 ]; then
    echo "FAIL: no ClientHello to a $SNI_MATCH host in the capture"
    head -5 "$WORK/tshark.err"
    exit 1
fi

sed 's/^/        /' "$WORK/offers.tsv"
echo

bad_offers=0
while IFS=$'\t' read -r stream sni groups; do
    case "$groups" in
        0x11ec*) ;;
        *)
            echo "  FAIL  stream $stream ($sni) did not offer $PQC_GROUP_NAME first: $groups"
            bad_offers=$((bad_offers + 1))
            ;;
    esac
done < "$WORK/offers.tsv"
if [ "$bad_offers" -eq 0 ]; then
    echo "  PASS  all $offered handshakes offered $PQC_GROUP_NAME first"
fi
echo

# --------------------------------------------------------------------------
echo "[3/3] What the server selected"
# --------------------------------------------------------------------------
# Only the streams identified above. A ServerHello carries no SNI, so the stream
# number is what ties it back to the host.
KVS_STREAMS="$(cut -f1 "$WORK/offers.tsv" | sort -un)"
STREAM_FILTER="$(echo "$KVS_STREAMS" | sed 's/^/tcp.stream == /' | paste -sd '|' - | sed 's/|/ || /g')"

"$TSHARK" -r "$PCAP" -Y "tls.handshake.type == 2 && ($STREAM_FILTER)" \
    -T fields -e tcp.stream -e tls.handshake.extensions_key_share_group \
    > "$WORK/selected.tsv" 2> "$WORK/tshark.err"

selected=$(wc -l < "$WORK/selected.tsv" | tr -d ' ')
if [ "$selected" -eq 0 ]; then
    echo "FAIL: no ServerHello on any $SNI_MATCH stream"
    head -5 "$WORK/tshark.err"
    exit 1
fi

classical=0
while IFS=$'\t' read -r stream group; do
    sni="$(awk -F'\t' -v s="$stream" '$1 == s {print $2; exit}' "$WORK/offers.tsv")"
    if [ "$group" = "$PQC_GROUP_DEC" ]; then
        echo "  PASS  stream $stream ($sni) negotiated $PQC_GROUP_NAME"
    else
        echo "  FAIL  stream $stream ($sni) negotiated group $group, not $PQC_GROUP_DEC"
        classical=$((classical + 1))
    fi
done < "$WORK/selected.tsv"
echo

if [ "$selected" -lt "$MIN_HANDSHAKES" ]; then
    echo "FAIL: only $selected completed handshakes, expected at least $MIN_HANDSHAKES."
    echo "      The samples probably never reached the service; this is not a pass."
    exit 1
fi

echo "=============================================================="
echo " $selected handshakes checked, $classical classical, $bad_offers bad offers"
echo "=============================================================="

if [ "$classical" -ne 0 ] || [ "$bad_offers" -ne 0 ]; then
    echo
    echo "A classical group on a KVS handshake means one of two things."
    echo "  1. The SDK stopped offering ML-KEM. Check the group list and the"
    echo "     AWS-LC version, and run scripts/check-awslc.sh."
    echo "  2. That endpoint stopped accepting it. Check the host in the line"
    echo "     above against the service rollout before changing any code."
    exit 1
fi

echo
echo "Every KVS handshake in this run was post-quantum."
exit 0
