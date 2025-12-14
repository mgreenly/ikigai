# REPL Terminal - Phase 6: Terminal Enhancements (Future Work)

[← Back to REPL Terminal Overview](README.md)

**Status**: Not implemented - future enhancement

**Goal**: Add bracketed paste mode and SGR color sequences for improved UX.

## Rationale

After Phase 5, the REPL has full functionality (input, input buffer, scrollback, scrolling). These proposed enhancements would add polish for real-world usage without changing core architecture:

**Bracketed paste mode** solves the classic "paste destroys formatting" problem - when users paste multi-line code, the REPL can't distinguish it from typed input, causing auto-indent and newline processing to mangle the content.

**SGR color sequences** provide visual distinction between user input and system output, improving readability and making the REPL feel more polished.

Both features are widely supported on modern Linux terminals (xterm, gnome-terminal, konsole, alacritty, kitty, wezterm).

## Proposed Implementation

### Feature 1: Bracketed Paste Mode

**Objective**: Enable safe pasting of multi-line content without formatting corruption.

#### What is Bracketed Paste Mode?

When enabled, the terminal wraps pasted content in escape sequences:
- Paste start: `\x1b[200~`
- Pasted content (raw bytes)
- Paste end: `\x1b[201~`

This lets the application distinguish typed vs pasted text and handle them differently.

#### Proposed Implementation Approach

**1. Enable bracketed paste on terminal initialization**

Would modify `src/terminal.c` in `ik_term_init()`:
```c
// After entering alternate screen, enable bracketed paste
const char *enable_bracketed_paste = "\x1b[?2004h";
if (posix_write_(tty_fd, enable_bracketed_paste, 8) < 0) {
    // Handle error
}
```

Would modify `ik_term_cleanup()`:
```c
// Before exiting alternate screen, disable bracketed paste
const char *disable_bracketed_paste = "\x1b[?2004l";
(void)posix_write_(ctx->tty_fd, disable_bracketed_paste, 8);
```

**2. Add paste action types to input parser**

Would add to `src/input.h`:
```c
typedef enum {
    // ... existing actions ...
    IK_INPUT_PASTE_START,  // \x1b[200~ received
    IK_INPUT_PASTE_END,    // \x1b[201~ received
} ik_input_action_type_t;
```

**3. Parse paste escape sequences**

Would modify `src/input.c` to recognize `\x1b[200~` and `\x1b[201~` sequences in escape sequence parsing logic.

**4. Track paste mode in REPL context**

Would add to `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    // ... existing fields ...
    bool in_paste_mode;  // True between PASTE_START and PASTE_END
} ik_repl_ctx_t;
```

**5. Handle paste mode in action processing**

Would modify `ik_repl_process_action()` in `src/repl.c`:
```c
case IK_INPUT_PASTE_START:
    repl->in_paste_mode = true;
    return OK(repl);

case IK_INPUT_PASTE_END:
    repl->in_paste_mode = false;
    return OK(repl);

// For other actions, check paste mode:
case IK_INPUT_NEWLINE:
    if (repl->in_paste_mode) {
        // Insert literal newline, don't trigger submit logic
        return ik_input_buffer_insert_newline(repl->input_buffer);
    } else {
        // Normal newline handling (might trigger submit in future)
        return ik_input_buffer_insert_newline(repl->input_buffer);
    }
```

**Proposed Test Coverage**:
- Parse `\x1b[200~` → `IK_INPUT_PASTE_START`
- Parse `\x1b[201~` → `IK_INPUT_PASTE_END`
- Multi-line paste preserves formatting
- Paste mode flag toggles correctly
- Newlines during paste are literal (not processed)
- Paste mode survives rendering cycles

**Estimated effort**: ~100-150 lines of implementation + tests

---

### Feature 2: SGR Color Sequences

**Objective**: Add visual distinction between user input and AI output using ANSI SGR color codes.

#### What are SGR Sequences?

SGR (Select Graphic Rendition) sequences change text appearance:
- `\x1b[31m` - Red foreground
- `\x1b[32m` - Green foreground
- `\x1b[34m` - Blue foreground
- `\x1b[1m` - Bold
- `\x1b[0m` - Reset to default

#### Color Scheme Design

**Minimal, readable scheme**:
- **User input buffer**: Default terminal colors (no modification)
- **Scrollback - User messages**: Cyan (`\x1b[36m`) - visually distinct but not harsh
- **Scrollback - AI responses**: Default (or subtle green `\x1b[32m` for affirmative tone)
- **Scrollback - System messages** (errors, status): Yellow (`\x1b[33m`)

**Rationale**: Subtle colors that work in both light and dark terminal themes.

#### Proposed Implementation Approach

**1. Add color configuration to render context**

Would add to `src/render.h`:
```c
typedef struct ik_render_ctx_t {
    // ... existing fields ...
    bool colors_enabled;  // Allow disabling for testing or user preference
} ik_render_ctx_t;
```

**2. Create SGR utility functions**

Would add to `src/render.c`:
```c
// SGR color codes
#define SGR_RESET     "\x1b[0m"
#define SGR_CYAN      "\x1b[36m"
#define SGR_GREEN     "\x1b[32m"
#define SGR_YELLOW    "\x1b[33m"

// Helper to wrap text in color
static void append_colored_text(char *buffer, size_t *offset,
                                 const char *color, const char *text,
                                 size_t text_len)
{
    // Append color code
    strcpy(buffer + *offset, color);
    *offset += strlen(color);

    // Append text
    memcpy(buffer + *offset, text, text_len);
    *offset += text_len;

    // Append reset
    strcpy(buffer + *offset, SGR_RESET);
    *offset += strlen(SGR_RESET);
}
```

**3. Update input buffer rendering**

Would keep input buffer in default colors (user is actively editing) or optionally add subtle bold for visual weight.

**4. Add scrollback line rendering with colors**

Would create:
```c
res_t ik_render_scrollback_line(ik_render_ctx_t *ctx,
                                       const char *text, size_t text_len,
                                       ik_message_type_t msg_type)
{
    const char *color = NULL;
    switch (msg_type) {
        case IK_MSG_USER:    color = SGR_CYAN; break;
        case IK_MSG_AI:      color = SGR_GREEN; break;
        case IK_MSG_SYSTEM:  color = SGR_YELLOW; break;
        default:             color = SGR_RESET; break;
    }

    // Render line with color wrapping
    // ...
}
```

**5. Add color toggle**

For accessibility and user preference:
```c
void ik_render_set_colors_enabled(ik_render_ctx_t *ctx, bool enabled);
```

**Proposed Test Coverage**:
- SGR codes correctly inserted for each message type
- Color reset applied after each line
- Colors can be disabled (returns plain text)
- No color codes in input buffer rendering
- Buffer size calculations account for color overhead
- Wide character + color rendering (ensure no corruption)

**Estimated effort**: ~150-200 lines of implementation + tests

---

## Testing Approach

### Unit Tests
Would test:
- Bracketed paste sequence parsing
- Paste mode state transitions
- SGR code generation
- Color toggling

### Integration Tests
Would verify:
- Paste multi-line code, no extra indentation
- Paste during mid-line edit, cursor position preserved
- Render scrollback with colors, correct ANSI output

### Manual Testing
Would test:
- Paste multi-line code and verify formatting preserved
- Submit messages and verify color distinction
- Verify readability in both light and dark themes

---

## Notes

**Why not implemented?**
- Non-essential features - core functionality complete in Phase 5
- Current REPL is production-ready without these enhancements
- Can be added later if needed based on user feedback

**Terminal compatibility**:
- Bracketed paste: xterm (2005+), all modern terminals
- SGR colors: Universal ANSI support
- Graceful degradation: If terminal doesn't support, sequences ignored

**Possible future enhancements** (beyond this proposal):
- 256-color or truecolor support
- Configurable color schemes
- Bold/italic/underline for emphasis
- Mouse tracking (unlikely to be needed for a REPL)

---

## Expected Benefits

**Bracketed paste**:
- Users could paste multi-line code without formatting corruption
- Paste mode would toggle correctly
- No performance impact during normal typing
- Works in all major Linux terminals

**Colors**:
- Visual distinction between user input and system output
- Readable in both light and dark terminal themes
- Can be disabled for accessibility
- No color artifacts or rendering glitches
