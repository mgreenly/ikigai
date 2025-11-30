#!/bin/bash
# Session End Hook - Log when sessions end

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping session_end logging" >&2
    exit 0
fi

# Extract session end information
REASON=$(echo "$INPUT" | jq -r '.reason // "unknown"')
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')

# Write to log
{
    echo "================================================================================"
    echo "Session Ended: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Reason: $REASON"
    echo "Session ID: $SESSION_ID"
    echo "================================================================================"
    echo ""
} >> "$CURRENT_LOG"

exit 0
