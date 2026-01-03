#!/bin/bash
# Session Start Hook - Rotate logs and initialize new session log

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"
ARCHIVE_DIR="$LOG_DIR/archive"

# Read JSON from stdin
INPUT=$(cat)

# Ensure directories exist
mkdir -p "$ARCHIVE_DIR"

# Rotate existing log if present
if [ -f "$CURRENT_LOG" ] && [ -s "$CURRENT_LOG" ]; then
    TIMESTAMP=$(date +%Y-%m-%d_%H%M%S)
    mv "$CURRENT_LOG" "$ARCHIVE_DIR/${TIMESTAMP}.log"
fi

# Clean up archives older than 30 days
find "$ARCHIVE_DIR" -name "*.log" -mtime +30 -delete 2>/dev/null || true

# Output JSONL: add event type and timestamp to input
echo "$INPUT" | jq -c '. + {event: "session_start", ts: "'"$(date -Iseconds)"'"}' > "$CURRENT_LOG"

exit 0
