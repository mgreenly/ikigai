#!/bin/bash
# Stop Hook - Log Claude's response from transcript

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin (Claude Code 2.X style)
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    exit 0
fi

# Get transcript path from stdin JSON
TRANSCRIPT_PATH=$(echo "$INPUT" | jq -r '.transcript_path // empty')

if [ -z "$TRANSCRIPT_PATH" ] || [ ! -f "$TRANSCRIPT_PATH" ]; then
    echo "[CLAUDE $(date '+%H:%M:%S')]" >> "$CURRENT_LOG"
    echo "(transcript not available)" >> "$CURRENT_LOG"
    echo "" >> "$CURRENT_LOG"
    exit 0
fi

# Extract the last assistant message from the transcript JSONL
# The transcript contains JSON lines - we look for the last assistant message
RESPONSE=$(tac "$TRANSCRIPT_PATH" 2>/dev/null | while read -r line; do
    # Check if this line is an assistant message
    ROLE=$(echo "$line" | jq -r '.role // .type // empty' 2>/dev/null)
    if [ "$ROLE" = "assistant" ]; then
        # Try to extract text content
        TEXT=$(echo "$line" | jq -r '
            if .content then
                if (.content | type) == "string" then
                    .content
                elif (.content | type) == "array" then
                    [.content[] | select(.type == "text") | .text] | join("\n")
                else
                    empty
                end
            elif .message then
                .message
            elif .text then
                .text
            else
                empty
            end
        ' 2>/dev/null)
        if [ -n "$TEXT" ] && [ "$TEXT" != "null" ]; then
            echo "$TEXT"
            break
        fi
    fi
done)

# Write to log
{
    echo "[CLAUDE $(date '+%Y-%m-%d %H:%M:%S')]"
    if [ -n "$RESPONSE" ]; then
        # Truncate very long responses (keep first 10000 chars)
        echo "$RESPONSE" | head -c 10000
        if [ ${#RESPONSE} -gt 10000 ]; then
            echo "... (truncated)"
        fi
    else
        echo "(response not captured)"
    fi
    echo ""
} >> "$CURRENT_LOG"

exit 0
