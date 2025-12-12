# Full Viewport Separator Bug

## Description

When scrollback fills the entire viewport, the bottom separator disappears and an extra blank line appears above the cursor. This only occurs when terminal height is smaller than scrollback content.

## Transcript

**Current behavior (incorrect):**

Terminal with 20 lines, scrollback has 15+ lines filling viewport:

```text
[scrollback line 1]
[scrollback line 2]
...
[scrollback line 14]
[scrollback line 15 - last visible]
────────────────────────────────────────────────────────────────────────────────

*
```

Issues:
- Extra blank line between top separator and cursor
- Bottom separator missing entirely

**Expected behavior (correct):**

```text
[scrollback line 1]
[scrollback line 2]
...
[scrollback line 14]
[scrollback line 15 - last visible]
────────────────────────────────────────────────────────────────────────────────
*
────────────────────────────────────────────────────────────────────────────────
```

Correct layout:
- No blank line between separator and cursor
- Bottom separator visible

**When terminal is large enough:**

If terminal has 30 lines and scrollback only has 10 lines, the layout renders correctly with both separators and no extra blank line. Bug only manifests when viewport is full.

## Walkthrough

1. User has scrollback content that fills or nearly fills the terminal viewport

2. Render system calculates layer positions:
   - Scrollback layer: lines 0 to N (fills most of screen)
   - Top separator: line N+1
   - Input layer: line N+2 (SHOULD BE N+1)
   - Bottom separator: line N+3 (SHOULD BE N+2)

3. **BUG:** When viewport is full, layer positioning calculation is off by one

4. Input layer renders one line too low, creating blank line

5. Bottom separator renders off-screen (beyond terminal height), so it's not visible

6. User sees malformed layout

## Reference

The layer cake rendering system should maintain consistent spacing regardless of scrollback size:

```c
// Expected layout (0-indexed line positions):
int32_t scrollback_end = scrollback_visible_lines - 1;
int32_t top_separator_line = scrollback_end + 1;
int32_t input_line = top_separator_line + 1;
int32_t bottom_separator_line = input_line + input_height;
```

The bug is likely in viewport/layer position calculations when:
- `scrollback_visible_lines + separator_lines + input_lines > term_height`

Investigate:
- `src/render.c` - layer positioning logic
- `src/layer_*.c` - individual layer rendering
- Viewport calculation when content overflows screen height
