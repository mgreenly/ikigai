#!/bin/bash
# Post Tool Use Hook - Log tool results after execution

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping post_tool_use logging" >&2
    exit 0
fi

# Extract tool information
TOOL_NAME=$(echo "$INPUT" | jq -r '.tool_name // "unknown"')
TOOL_USE_ID=$(echo "$INPUT" | jq -r '.tool_use_id // "unknown"')
TOOL_RESPONSE=$(echo "$INPUT" | jq -r '.tool_response // "no response"')

# Write to log
{
    echo "[TOOL RESULT $(date '+%Y-%m-%d %H:%M:%S')] $TOOL_NAME (ID: $TOOL_USE_ID)"
    # Truncate very long responses
    if [ ${#TOOL_RESPONSE} -gt 5000 ]; then
        echo "$TOOL_RESPONSE" | head -c 5000
        echo "... (truncated - ${#TOOL_RESPONSE} total chars)"
    else
        echo "$TOOL_RESPONSE"
    fi
    echo ""
} >> "$CURRENT_LOG"

exit 0
