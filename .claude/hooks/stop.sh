#!/bin/bash
# Stop Hook - Log Claude's response from transcript

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log directory exists
mkdir -p "$LOG_DIR"

# Output JSONL: add event type and timestamp to input
echo "$INPUT" | jq -c '. + {event: "stop", ts: "'"$(date -Iseconds)"'"}' >> "$CURRENT_LOG"

exit 0
