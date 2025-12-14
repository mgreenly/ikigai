# Hook Data Formats

All hooks receive data as JSON via stdin. This document details the schemas for each hook type.

## General Pattern

```bash
#!/bin/bash
# Read JSON from stdin
INPUT=$(cat)

# Parse with jq
FIELD=$(echo "$INPUT" | jq -r '.field_name // "default"')
```

---

## SessionStart Data

```json
{
  "source": "string",
  "session_id": "string"
}
```

### Fields

**source** (string):
- Where the session was initiated from
- Examples: `"cli"`, `"api"`, etc.
- Use `// "unknown"` as default

**session_id** (string):
- Unique identifier for this session
- Useful for correlating events across hooks
- Use `// "unknown"` as default

### Example

```json
{
  "source": "cli",
  "session_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

### Parsing Example

```bash
SOURCE=$(echo "$INPUT" | jq -r '.source // "unknown"')
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')

echo "Session $SESSION_ID started from $SOURCE"
```

---

## UserPromptSubmit Data

```json
{
  "prompt": "string"
}
```

### Fields

**prompt** (string):
- The exact text the user submitted
- May contain newlines, special characters
- Use `// empty` to handle missing prompts

### Example

```json
{
  "prompt": "What is 2 + 2"
}
```

### Parsing Example

```bash
PROMPT=$(echo "$INPUT" | jq -r '.prompt // empty')

if [ -n "$PROMPT" ]; then
    echo "[USER] $PROMPT" >> log.txt
fi
```

---

## Stop Data

```json
{
  "transcript_path": "string"
}
```

### Fields

**transcript_path** (string):
- Absolute path to the session transcript file
- JSONL format (JSON Lines - one JSON object per line)
- May not exist or be inaccessible
- Use `// empty` as default

### Example

```json
{
  "transcript_path": "/tmp/claude-session-abc123/transcript.jsonl"
}
```

### Parsing Example

```bash
TRANSCRIPT_PATH=$(echo "$INPUT" | jq -r '.transcript_path // empty')

if [ -z "$TRANSCRIPT_PATH" ] || [ ! -f "$TRANSCRIPT_PATH" ]; then
    echo "Transcript not available"
    exit 0
fi

# Process transcript...
```

---

## Transcript Format (JSONL)

The transcript file is in JSONL (JSON Lines) format - each line is a complete JSON object representing a message in the conversation.

### Message Structure

Based on Anthropic's API format:

```json
{
  "role": "user|assistant",
  "content": "string or array"
}
```

### Content Types

**Simple text content**:
```json
{
  "role": "assistant",
  "content": "The answer is 4"
}
```

**Structured content** (array of content blocks):
```json
{
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "The answer is 4"
    },
    {
      "type": "tool_use",
      "id": "...",
      "name": "calculator",
      "input": {...}
    }
  ]
}
```

### Extracting Latest Assistant Message

The transcript contains the full conversation history. To get Claude's latest response:

```bash
# Method 1: Using tac (reverse file) - finds last assistant message
RESPONSE=$(tac "$TRANSCRIPT_PATH" | while read -r line; do
    ROLE=$(echo "$line" | jq -r '.role // empty')
    if [ "$ROLE" = "assistant" ]; then
        # Handle both string and array content
        echo "$line" | jq -r '
            if .content then
                if (.content | type) == "string" then
                    .content
                elif (.content | type) == "array" then
                    [.content[] | select(.type == "text") | .text] | join("\n")
                else
                    empty
                end
            else
                empty
            end
        '
        break
    fi
done)

# Method 2: Filter all assistant messages, take last
RESPONSE=$(cat "$TRANSCRIPT_PATH" | \
    jq -r 'select(.role == "assistant") |
           if (.content | type) == "string" then .content
           elif (.content | type) == "array" then
               [.content[] | select(.type == "text") | .text] | join("\n")
           else empty end' | \
    tail -1)
```

### Extracting All Messages

```bash
# Get all user prompts
cat "$TRANSCRIPT_PATH" | jq -r 'select(.role == "user") | .content'

# Get conversation as readable text
cat "$TRANSCRIPT_PATH" | jq -r '
  if .role == "user" then
    "[USER] " + .content
  elif .role == "assistant" then
    if (.content | type) == "string" then
      "[CLAUDE] " + .content
    elif (.content | type) == "array" then
      "[CLAUDE] " + ([.content[] | select(.type == "text") | .text] | join("\n"))
    else
      ""
    end
  else
    ""
  end
'
```

---

## Field Handling Best Practices

### Use Defaults for Missing Fields

```bash
# Good - handles missing fields gracefully
FIELD=$(echo "$INPUT" | jq -r '.field // "default"')

# Bad - fails if field is missing
FIELD=$(echo "$INPUT" | jq -r '.field')
```

### Check for Empty/Null Values

```bash
TRANSCRIPT_PATH=$(echo "$INPUT" | jq -r '.transcript_path // empty')

if [ -z "$TRANSCRIPT_PATH" ]; then
    echo "No transcript path provided"
    exit 0
fi
```

### Validate File Paths

```bash
if [ ! -f "$TRANSCRIPT_PATH" ]; then
    echo "Transcript file not found: $TRANSCRIPT_PATH"
    exit 0
fi

if [ ! -r "$TRANSCRIPT_PATH" ]; then
    echo "Transcript file not readable: $TRANSCRIPT_PATH"
    exit 0
fi
```

### Handle Multi-line Content

```bash
# Preserve newlines in prompts
PROMPT=$(echo "$INPUT" | jq -r '.prompt // empty')

# When writing to logs, ensure proper quoting
{
    echo "[USER $(date)]"
    echo "$PROMPT"  # Preserves newlines
    echo ""
} >> log.txt
```

---

## Error Cases

### Missing Data

Hooks should handle missing or invalid data gracefully:

```bash
#!/bin/bash
set -e

INPUT=$(cat)

# Validate JSON
if ! echo "$INPUT" | jq '.' > /dev/null 2>&1; then
    echo "Invalid JSON input" >&2
    exit 0  # Non-blocking
fi

# Extract with defaults
FIELD=$(echo "$INPUT" | jq -r '.field // "default"')

exit 0
```

### Malformed Transcript

```bash
# Check if transcript is valid JSONL
if [ -f "$TRANSCRIPT_PATH" ]; then
    if ! head -1 "$TRANSCRIPT_PATH" | jq '.' > /dev/null 2>&1; then
        echo "Malformed transcript" >&2
        exit 0
    fi
fi
```

---

## Testing Hooks

### Test with Sample Data

Create test JSON files:

**session_start_test.json**:
```json
{
  "source": "cli",
  "session_id": "test-123"
}
```

**user_prompt_test.json**:
```json
{
  "prompt": "Test prompt with\nmultiple lines"
}
```

**stop_test.json**:
```json
{
  "transcript_path": "/path/to/test_transcript.jsonl"
}
```

### Run Tests

```bash
# Test SessionStart hook
cat session_start_test.json | ./hooks/session_start.sh

# Test UserPromptSubmit hook
cat user_prompt_test.json | ./hooks/user_prompt.sh

# Test Stop hook
cat stop_test.json | ./hooks/stop.sh
```

### Create Test Transcript

**test_transcript.jsonl**:
```jsonl
{"role":"user","content":"What is 2 + 2"}
{"role":"assistant","content":"4"}
{"role":"user","content":"What about 3 + 3"}
{"role":"assistant","content":[{"type":"text","text":"The answer is 6"}]}
```
