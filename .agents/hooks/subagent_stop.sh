#!/bin/bash
# Subagent Stop Hook - Log when sub-agents complete

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin
INPUT=$(cat)

# Ensure log file exists
if [ ! -f "$CURRENT_LOG" ]; then
    mkdir -p "$LOG_DIR"
    echo "Log file not found, skipping subagent_stop logging" >&2
    exit 0
fi

# Extract session info
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')
TRANSCRIPT_PATH=$(echo "$INPUT" | jq -r '.transcript_path // empty')

# Try to extract the subagent's final response from the transcript
SUBAGENT_RESPONSE=""
if [ -n "$TRANSCRIPT_PATH" ] && [ -f "$TRANSCRIPT_PATH" ]; then
    SUBAGENT_RESPONSE=$(tac "$TRANSCRIPT_PATH" 2>/dev/null | while read -r line; do
        ROLE=$(echo "$line" | jq -r '.role // .type // empty' 2>/dev/null)
        if [ "$ROLE" = "assistant" ]; then
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
fi

# Write to log
{
    echo "[SUBAGENT COMPLETED $(date '+%Y-%m-%d %H:%M:%S')] Session: $SESSION_ID"
    if [ -n "$SUBAGENT_RESPONSE" ]; then
        # Truncate very long responses
        if [ ${#SUBAGENT_RESPONSE} -gt 5000 ]; then
            echo "$SUBAGENT_RESPONSE" | head -c 5000
            echo "... (truncated - ${#SUBAGENT_RESPONSE} total chars)"
        else
            echo "$SUBAGENT_RESPONSE"
        fi
    else
        echo "(subagent response not captured)"
    fi
    echo ""
} >> "$CURRENT_LOG"

exit 0
