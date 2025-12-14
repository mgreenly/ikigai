# Hook Types and Lifecycle

Claude Code provides three hook types that cover the session lifecycle.

## Hook Types

### SessionStart

**Triggered**: When a new Claude Code session begins

**Timing**: Before the first user prompt is processed

**Common Uses**:
- Initialize session logging
- Rotate previous logs
- Clean up old data
- Send session start notifications
- Set up session-specific resources

**Input Data** (via stdin JSON):
```json
{
  "source": "cli|api|...",
  "session_id": "unique-session-identifier"
}
```

**Example Flow**:
```
User starts Claude Code
    ↓
SessionStart hook triggered
    ↓
Hook rotates old logs, creates new session log
    ↓
Session ready for prompts
```

---

### UserPromptSubmit

**Triggered**: When the user submits a prompt to Claude

**Timing**: After user input is captured, before Claude processes it

**Common Uses**:
- Log user prompts
- Track user activity
- Implement prompt filtering/validation
- Trigger analytics
- Monitor for specific keywords

**Input Data** (via stdin JSON):
```json
{
  "prompt": "The user's input text"
}
```

**Example Flow**:
```
User types and submits prompt
    ↓
UserPromptSubmit hook triggered
    ↓
Hook logs prompt with timestamp
    ↓
Claude processes prompt and responds
```

---

### Stop

**Triggered**: When Claude finishes generating a response

**Timing**: After Claude's response is complete and displayed

**Common Uses**:
- Log Claude's responses
- Extract and process Claude's output
- Trigger post-response workflows
- Analyze response content
- Update conversation transcripts

**Input Data** (via stdin JSON):
```json
{
  "transcript_path": "/path/to/session/transcript.jsonl"
}
```

**Important Notes**:
- The `transcript_path` points to a JSONL file containing the full conversation
- Each line in the transcript is a JSON object representing a message
- You need to parse the transcript to extract Claude's latest response
- The transcript format uses the Anthropic API message structure

**Example Flow**:
```
Claude completes response
    ↓
Response displayed to user
    ↓
Stop hook triggered
    ↓
Hook reads transcript, extracts last assistant message
    ↓
Hook logs response to session log
```

---

## Hook Lifecycle

### Complete Session Flow

```
1. Claude Code starts
       ↓
2. SessionStart hook(s) run
       ↓
3. User enters prompt
       ↓
4. UserPromptSubmit hook(s) run
       ↓
5. Claude processes and responds
       ↓
6. Stop hook(s) run
       ↓
7. Back to step 3 (or exit if session ends)
```

### Hook Execution Order

When multiple hooks are defined for the same type:
1. Hooks execute sequentially in array order
2. Each hook runs to completion before the next
3. Hook failures are non-blocking (next hook still runs)
4. Timeout applies per-hook, not total

### Error Handling

**Non-blocking failures**: Hook errors don't interrupt the session
- Exit code != 0: Logged as error, session continues
- Timeout exceeded: Hook killed, session continues
- Script not found: Error logged, session continues
- Permission denied: Error logged, session continues

**Error Messages**: Displayed in Claude Code output
```
Ran 1 stop hook
  ⎿  Stop hook error: Failed with non-blocking status code: <error>
```

---

## Implementation Guidelines

### SessionStart Hooks

```bash
#!/bin/bash
set -e

# Read JSON input
INPUT=$(cat)

# Extract data
SOURCE=$(echo "$INPUT" | jq -r '.source // "unknown"')
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')

# Initialize resources
echo "Session started: $SESSION_ID from $SOURCE"

exit 0
```

### UserPromptSubmit Hooks

```bash
#!/bin/bash
set -e

# Read JSON input
INPUT=$(cat)

# Extract prompt
PROMPT=$(echo "$INPUT" | jq -r '.prompt // empty')

# Log the prompt
echo "[$(date)] USER: $PROMPT" >> session.log

exit 0
```

### Stop Hooks

```bash
#!/bin/bash
set -e

# Read JSON input
INPUT=$(cat)

# Get transcript path
TRANSCRIPT_PATH=$(echo "$INPUT" | jq -r '.transcript_path // empty')

# Extract last assistant message from JSONL transcript
if [ -f "$TRANSCRIPT_PATH" ]; then
    RESPONSE=$(tac "$TRANSCRIPT_PATH" | while read -r line; do
        ROLE=$(echo "$line" | jq -r '.role // empty')
        if [ "$ROLE" = "assistant" ]; then
            echo "$line" | jq -r '.content'
            break
        fi
    done)

    echo "[$(date)] CLAUDE: $RESPONSE" >> session.log
fi

exit 0
```

---

## Best Practices

1. **Always read from stdin**: Don't rely on command-line arguments
2. **Use `jq` for JSON parsing**: Robust and handles edge cases
3. **Provide defaults**: Use `// "default"` in jq for missing fields
4. **Exit 0 on success**: Proper exit codes for error tracking
5. **Keep hooks fast**: User experience suffers with slow hooks
6. **Handle missing data**: Transcript path might not exist
7. **Set -e for safety**: Exit on errors unless intentional
