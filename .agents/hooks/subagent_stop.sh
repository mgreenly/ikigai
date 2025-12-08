#!/bin/bash
# Subagent Stop Hook - Log when sub-agents complete

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log directory exists
mkdir -p "$LOG_DIR"

# Output JSONL: add event type and timestamp to input
echo "$INPUT" | jq -c '. + {event: "subagent_stop", ts: "'"$(date -Iseconds)"'"}' >> "$CURRENT_LOG"

exit 0
