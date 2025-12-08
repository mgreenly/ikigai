#!/bin/bash
# Post Tool Use Hook - Log tool results after execution

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log directory exists
mkdir -p "$LOG_DIR"

# Output JSONL: add event type and timestamp to input
echo "$INPUT" | jq -c '. + {event: "post_tool_use", ts: "'"$(date -Iseconds)"'"}' >> "$CURRENT_LOG"

exit 0
