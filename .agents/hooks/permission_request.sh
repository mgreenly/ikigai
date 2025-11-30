#!/bin/bash
# Permission Request Hook - Log when permission dialogs are shown

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping permission_request logging" >&2
    exit 0
fi

# Extract permission information
TOOL_NAME=$(echo "$INPUT" | jq -r '.tool_name // "unknown"')
TOOL_INPUT=$(echo "$INPUT" | jq -c '.tool_input // {}')

# Write to log
{
    echo "[PERMISSION REQUEST $(date '+%Y-%m-%d %H:%M:%S')] $TOOL_NAME"
    echo "Input: $TOOL_INPUT"
    echo ""
} >> "$CURRENT_LOG"

exit 0
