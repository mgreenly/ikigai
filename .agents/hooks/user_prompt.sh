#!/bin/bash
# User Prompt Submit Hook - Log user prompts

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin (Claude Code 2.X style)
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    echo "Session log not found, creating..." >&2
    mkdir -p "$LOG_DIR"
    echo "================================================================================
Session Started: $(date '+%Y-%m-%d %H:%M:%S')
Source: resumed (log recovered)
================================================================================
" > "$CURRENT_LOG"
fi

# Extract prompt from stdin JSON
PROMPT=$(echo "$INPUT" | jq -r '.prompt // empty')

if [ -n "$PROMPT" ]; then
    {
        echo "[USER $(date '+%Y-%m-%d %H:%M:%S')]"
        echo "$PROMPT"
        echo ""
    } >> "$CURRENT_LOG"
fi

exit 0
