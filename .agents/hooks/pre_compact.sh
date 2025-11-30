#!/bin/bash
# Pre Compact Hook - Log before compact operations

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping pre_compact logging" >&2
    exit 0
fi

# Extract compact information
TRIGGER=$(echo "$INPUT" | jq -r '.trigger // "unknown"')
CUSTOM_INSTRUCTIONS=$(echo "$INPUT" | jq -r '.custom_instructions // empty')

# Write to log
{
    echo "[COMPACT OPERATION $(date '+%Y-%m-%d %H:%M:%S')]"
    echo "Trigger: $TRIGGER"
    if [ -n "$CUSTOM_INSTRUCTIONS" ]; then
        echo "Custom Instructions: $CUSTOM_INSTRUCTIONS"
    fi
    echo ""
} >> "$CURRENT_LOG"

exit 0
