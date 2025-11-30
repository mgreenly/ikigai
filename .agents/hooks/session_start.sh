#!/bin/bash
# Session Start Hook - Rotate logs and initialize new session log

set -e

LOG_DIR="$(dirname "$0")/../logs"
CURRENT_LOG="$LOG_DIR/current.log"

# Read JSON from stdin (Claude Code 2.X style)
INPUT=$(cat)
ARCHIVE_DIR="$LOG_DIR/archive"

# Ensure directories exist
mkdir -p "$ARCHIVE_DIR"

# Rotate existing log if present
if [ -f "$CURRENT_LOG" ] && [ -s "$CURRENT_LOG" ]; then
    TIMESTAMP=$(date +%Y-%m-%d_%H%M%S)
    mv "$CURRENT_LOG" "$ARCHIVE_DIR/${TIMESTAMP}.log"
fi

# Clean up archives older than 30 days
find "$ARCHIVE_DIR" -name "*.log" -mtime +30 -delete 2>/dev/null || true

# Parse session info from stdin JSON
SOURCE=$(echo "$INPUT" | jq -r '.source // "unknown"')
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')

# Create new session log
cat > "$CURRENT_LOG" << EOF
================================================================================
Session Started: $(date '+%Y-%m-%d %H:%M:%S')
Source: $SOURCE
Session ID: $SESSION_ID
================================================================================

EOF

exit 0
