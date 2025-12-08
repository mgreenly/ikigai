# Readline Features

This document captures the design for command history and tab completion features.

## Command History

### Storage

- **File**: `.ikigai_history` in current working directory (project-local)
- **Format**: JSONL (one JSON object per line)
- **Size**: Configurable via `history_size` in `~/.config/ikigai/config.json`

```jsonl
{"cmd": "/clear", "ts": "2025-01-15T10:30:00Z"}
{"cmd": "explain this function\nand its edge cases", "ts": "2025-01-15T10:31:00Z"}
{"cmd": "/model gpt-4o", "ts": "2025-01-15T10:32:00Z"}
```

Multi-line input is preserved via JSON string escaping (`\n`).

### Navigation

- **Trigger**: Cursor at position 0 (empty buffer or cursor at start)
- **Up**: Previous command from history
- **Down**: Next command (or back to empty/pending input)
- Otherwise: Up/Down perform multi-line cursor movement (existing behavior)

### Behavior

- History loaded on REPL startup
- New command appended to file on each submission
- Consecutive duplicate commands not stored
- User's pending input preserved when browsing (restored on Down past end)

### Data Structure

```c
typedef struct {
    char **entries;       // Array of past commands
    size_t count;         // Number of entries
    size_t capacity;      // Allocated size
    size_t index;         // Current browsing position
    char *pending;        // What user was typing before browsing
} ik_history_t;
```

## Tab Completion

### Trigger

- Tab key when input starts with `/`
- Case-sensitive matching

### Display

- Completions shown below input buffer (new layer)
- Maximum 10 suggestions
- Alphabetically sorted
- Format: `  command   description`

```
┌─────────────────────────────┐
│ Scrollback                  │
│ ...                         │
│ last visible message        │
├─────────────────────────────┤  ← upper separator
│ > /m█                       │  ← input buffer
├─────────────────────────────┤  ← lower separator (new)
│   mark   Create checkpoint  │  ← completions
│   model  Switch LLM model   │
└─────────────────────────────┘
```

### Viewport Behavior

When completions are visible, viewport shifts down to keep input and completions in view.

**Priority order when space is tight:**
1. Input buffer (must always be fully visible)
2. Completions (show as many as fit, up to 10)
3. Scrollback (gets compressed/scrolled)

### Interaction

- Arrow keys navigate through completion list
- Tab accepts current selection
- Continue typing filters the list
- Escape dismisses completions

### Argument Completion

| Command | Completions |
|---------|-------------|
| `/model` | Available models from registry |
| `/rewind` | Existing mark labels |
| `/debug` | `on`, `off` |
| `/mark` | None (freeform label) |
| `/system` | None (freeform text) |
| `/clear` | None |
| `/help` | None |

### Data Structure

```c
typedef struct {
    const char **candidates;  // Matching commands/args
    size_t count;
    size_t current;           // Highlighted index
} ik_completion_t;
```

## UI Changes

### Horizontal Rules

Add horizontal rules above and below the input area:
- Upper rule: separates scrollback from input (existing separator)
- Lower rule: separates input from completions (new)

Both rules frame the input area as a distinct zone.

## New Input Action

```c
IK_INPUT_TAB  // Tab key for completion
```

## State Ownership

- `ik_repl_ctx_t` owns `ik_history_t *history`
- `ik_repl_ctx_t` owns `ik_completion_t *completion` (NULL when inactive)
- Completion layer reads from `repl->completion`

## Implementation Notes

### Completion Layer

New layer in the layer stack that:
- Renders below input layer
- Only visible when `repl->completion != NULL`
- Handles its own visibility state

### History Persistence

- Load: Parse JSONL file on `ik_repl_init()`
- Save: Append single line on command submission
- Respect `history_size` limit (truncate oldest on load if exceeded)
