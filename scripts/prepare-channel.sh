#!/bin/bash
#
# Creates a KVS signaling channel if it doesn't already exist.
#
# Usage: ./prepare-channel.sh <channel-name>

set -euo pipefail

CHANNEL_NAME="${1:?Usage: $0 <channel-name>}"

if aws kinesisvideo describe-signaling-channel --channel-name "$CHANNEL_NAME" 2>/dev/null; then
    echo "Channel $CHANNEL_NAME already exists"
else
    echo "Creating channel $CHANNEL_NAME"
    aws kinesisvideo create-signaling-channel --channel-name "$CHANNEL_NAME"
fi
