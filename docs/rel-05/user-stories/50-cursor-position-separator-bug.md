# Cursor Renders On Separator Instead Of Input Line

## Description

When viewport has exactly one blank line at the bottom and user types in input buffer, the cursor renders on the bottom separator line instead of on the input line where the text is being typed.

## Transcript

**Current behavior (incorrect):**

Terminal with scrollback filling most of viewport, exactly one blank line at bottom:

```text
[scrollback line 1]
[scrollback line 2]
...
[scrollback second-to-last line]
[scrollback last line]

────────────────────────────────────────────────────────────────────────────────
/clear
──────*─────────────────────────────────────────────────────────────────────────
```

Issues:
- Text "/clear" appears on correct line (input line)
- Cursor "*" appears on wrong line (bottom separator line)
- Cursor should be after "r" in "/clear", not on separator

**Expected behavior (correct):**

```text
[scrollback line 1]
[scrollback line 2]
...
[scrollback second-to-last line]
[scrollback last line]

────────────────────────────────────────────────────────────────────────────────
/clear*
────────────────────────────────────────────────────────────────────────────────
```

Correct layout:
- Text and cursor both on input line
- Cursor immediately after last character
- Bottom separator on its own line below

## Walkthrough

1. Scrollback content fills viewport leaving exactly one blank line at bottom

2. User types "/clear" in input buffer

3. Input layer renders text at correct Y position (after top separator)

4. **BUG:** Cursor Y position calculated incorrectly, places cursor one line too low

5. Cursor renders on bottom separator line instead of input line

6. Bottom separator renders with cursor overlaid on it

7. User sees text on correct line but cursor displaced below

## Reference

Cursor position should be calculated relative to input layer position:

```c
// Correct cursor positioning:
int32_t cursor_y = input_layer_y;  // Same line as input text
int32_t cursor_x = input_text_width;  // After last character
```

Bug is likely:
```c
// WRONG - adds extra offset
int32_t cursor_y = input_layer_y + 1;  // Off by one
```

Investigate:
- `src/layer_input.c` - cursor rendering logic
- `src/render.c` - cursor position calculation
- Input layer height calculations when viewport is constrained
