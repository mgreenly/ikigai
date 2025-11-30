#!/bin/bash
# Pre Tool Use Hook - Log tool calls before execution

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping pre_tool_use logging" >&2
    exit 0
fi

# Extract tool information
TOOL_NAME=$(echo "$INPUT" | jq -r '.tool_name // "unknown"')
TOOL_USE_ID=$(echo "$INPUT" | jq -r '.tool_use_id // "unknown"')
TOOL_INPUT=$(echo "$INPUT" | jq -c '.tool_input // {}')

# Write to log
{
    echo "[TOOL CALL $(date '+%Y-%m-%d %H:%M:%S')] $TOOL_NAME (ID: $TOOL_USE_ID)"
    echo "Input: $TOOL_INPUT"
    echo ""
} >> "$CURRENT_LOG"

exit 0
