# REPL Terminal - Phase 6: Terminal Enhancements

[← Back to REPL Terminal Overview](README.md)

**Goal**: Add bracketed paste mode and SGR color sequences for improved UX.

**Status**: Not started (post-Phase 5)

## Rationale

After Phase 5, the REPL will have full functionality (input, input buffer, scrollback, scrolling). These enhancements add polish for real-world usage without changing core architecture:

**Bracketed paste mode** solves the classic "paste destroys formatting" problem - when users paste multi-line code, the REPL can't distinguish it from typed input, causing auto-indent and newline processing to mangle the content.

**SGR color sequences** provide visual distinction between user input and system output, improving readability and making the REPL feel more polished.

Both features are widely supported on modern Linux terminals (xterm, gnome-terminal, konsole, alacritty, kitty, wezterm).

## Implementation Tasks

### Task 6.1: Bracketed Paste Mode

**Goal**: Enable safe pasting of multi-line content without formatting corruption.

#### What is Bracketed Paste Mode?

When enabled, the terminal wraps pasted content in escape sequences:
- Paste start: `\x1b[200~`
- Pasted content (raw bytes)
- Paste end: `\x1b[201~`

This lets the application distinguish typed vs pasted text and handle them differently.

#### Implementation Steps

**1. Enable bracketed paste on terminal initialization**

Modify `src/terminal.c` in `ik_term_init()`:
```c
// After entering alternate screen, enable bracketed paste
const char *enable_bracketed_paste = "\x1b[?2004h";
if (ik_write_wrapper(tty_fd, enable_bracketed_paste, 8) < 0) {
    // Handle error
}
```

Modify `ik_term_cleanup()`:
```c
// Before exiting alternate screen, disable bracketed paste
const char *disable_bracketed_paste = "\x1b[?2004l";
(void)ik_write_wrapper(ctx->tty_fd, disable_bracketed_paste, 8);
```

**2. Add paste action types to input parser**

Modify `src/input.h`:
```c
typedef enum {
    // ... existing actions ...
    IK_INPUT_PASTE_START,  // \x1b[200~ received
    IK_INPUT_PASTE_END,    // \x1b[201~ received
} ik_input_action_type_t;
```

**3. Parse paste escape sequences**

Modify `src/input.c` to recognize `\x1b[200~` and `\x1b[201~` sequences in escape sequence parsing logic.

**4. Track paste mode in REPL context**

Modify `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    // ... existing fields ...
    bool in_paste_mode;  // True between PASTE_START and PASTE_END
} ik_repl_ctx_t;
```

**5. Handle paste mode in action processing**

Modify `ik_repl_process_action()` in `src/repl.c`:
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

**Test Coverage** (`tests/unit/input/input_paste_test.c`):
- Parse `\x1b[200~` → `IK_INPUT_PASTE_START`
- Parse `\x1b[201~` → `IK_INPUT_PASTE_END`
- Multi-line paste preserves formatting
- Paste mode flag toggles correctly
- Newlines during paste are literal (not processed)
- Paste mode survives rendering cycles

**Estimated size**: ~100-150 lines of implementation + tests

---

### Task 6.2: SGR Color Sequences

**Goal**: Add visual distinction between user input and AI output using ANSI SGR color codes.

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

#### Implementation Steps

**1. Add color configuration to render context**

Modify `src/render.h`:
```c
typedef struct ik_render_ctx_t {
    // ... existing fields ...
    bool colors_enabled;  // Allow disabling for testing or user preference
} ik_render_ctx_t;
```

**2. Create SGR utility functions**

Add to `src/render.c`:
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

Modify `ik_render_input_buffer()`:
- Keep input buffer in default colors (user is actively editing)
- Or optionally add subtle bold for visual weight

**4. Add scrollback line rendering with colors**

When Phase 4 adds scrollback rendering, create:
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

**Test Coverage** (`tests/unit/render/colors_test.c`):
- SGR codes correctly inserted for each message type
- Color reset applied after each line
- Colors can be disabled (returns plain text)
- No color codes in input buffer rendering
- Buffer size calculations account for color overhead
- Wide character + color rendering (ensure no corruption)

**Estimated size**: ~150-200 lines of implementation + tests

---

## Testing Strategy

### Unit Tests
- Bracketed paste sequence parsing
- Paste mode state transitions
- SGR code generation
- Color toggling

### Integration Tests
- Paste multi-line code, verify no extra indentation
- Paste during mid-line edit, verify cursor position preserved
- Render scrollback with colors, verify correct ANSI output

### Manual Testing
```bash
# Test bracketed paste
1. Run ikigai REPL
2. Copy multi-line Python code with indentation
3. Paste into input buffer
4. Verify formatting preserved exactly

# Test colors
1. Run ikigai REPL
2. Submit several messages (creating scrollback)
3. Verify user messages appear in cyan
4. Verify distinct visual separation from AI responses
```

---

## Notes

**Why after Phase 5?**
- Non-essential features - core functionality must work first
- Cleaner to add after architecture is stable
- Can be shipped incrementally (paste first, colors later)

**Terminal compatibility**:
- Bracketed paste: xterm (2005+), all modern terminals
- SGR colors: Universal ANSI support
- Graceful degradation: If terminal doesn't support, sequences ignored

**Future enhancements** (not in scope for Phase 6):
- 256-color or truecolor support
- Configurable color schemes
- Bold/italic/underline for emphasis
- Mouse tracking (probably never needed for a REPL)

---

## Success Criteria

**Bracketed paste**:
- ✅ Users can paste multi-line code without formatting corruption
- ✅ Paste mode toggles correctly
- ✅ No performance impact during normal typing
- ✅ Works in all major Linux terminals

**Colors**:
- ✅ Visual distinction between user input and AI output
- ✅ Readable in both light and dark terminal themes
- ✅ Can be disabled for accessibility
- ✅ No color artifacts or rendering glitches
