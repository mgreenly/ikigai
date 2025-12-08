# Input Layer Missing Trailing Newline

## Problem

When typing in the input buffer, text appears ON the separator line instead of below it.

**Expected:**
```
----------------------------------------------------------------------------------------
/
----------------------------------------------------------------------------------------
```

**Actual:**
```
----------------------------------------------------------------------------------------
/----------------------------------------------------------------------------------------
```

The slash (and any input text) renders on the same visual line as the upper separator, making it appear that the input has merged with the separator.

## Root Cause

The input layer (`layer_input.c`) does not append a trailing `\r\n` after non-empty input text.

The layer cake architecture renders layers in sequence:
1. Scrollback layer
2. Spinner layer
3. **Separator layer** - outputs `--------\r\n`
4. **Input layer** - outputs just the text WITHOUT trailing newline
5. Lower separator layer
6. Completion layer

When the separator outputs `----\r\n`, the cursor moves to the next line. But when the input layer outputs just `/` without a trailing newline, the next layer's content (or the terminal's line wrap behavior) doesn't start on a fresh line.

The empty input case correctly outputs `\r\n` to produce a blank line. The non-empty case does not.

## Why This Matters

- The input area is visually indistinct from the separator
- Users cannot see where their input begins
- The lower separator and completion menu will also render incorrectly
- Violates the layer cake principle that each layer is responsible for its own vertical space

## Proposed Fix

The input layer should ensure its output ends with `\r\n` so subsequent layers start on a fresh line. This matches the contract implied by `get_height()` - if the layer claims N rows, it should output content that occupies exactly N rows.

Two considerations:
1. Input text that already ends with `\n` should not get double newlines
2. Multi-line input needs `\n` → `\r\n` conversion (already done) plus a final `\r\n`

## Scope

- Single file change: `src/layer_input.c`
- Affects the `input_render()` function
- Existing tests for input layer rendering need review
- May need new test case for single-character input rendering

## Related

- Layer cake architecture: `src/layer.h`, `src/layer.c`
- Similar layer: `layer_separator.c` (correctly appends `\r\n`)
- Viewport calculation: `src/repl_viewport.c`
