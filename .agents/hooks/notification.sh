#!/bin/bash
# Notification Hook - Log notifications

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping notification logging" >&2
    exit 0
fi

# Extract notification information
MESSAGE=$(echo "$INPUT" | jq -r '.message // "no message"')
NOTIFICATION_TYPE=$(echo "$INPUT" | jq -r '.notification_type // "unknown"')

# Write to log
{
    echo "[NOTIFICATION $(date '+%Y-%m-%d %H:%M:%S')] Type: $NOTIFICATION_TYPE"
    echo "$MESSAGE"
    echo ""
} >> "$CURRENT_LOG"

exit 0
