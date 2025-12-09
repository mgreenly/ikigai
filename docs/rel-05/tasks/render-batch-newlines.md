# Task: Batch Newline Conversion in Layer Rendering

## Target
Render Performance Optimization - Eliminate character-by-character append overhead

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/layer_scrollback.c (lines 87-93 - character-by-character newline conversion)
- src/layer_input.c (check for similar patterns)
- src/layer.c (ik_output_buffer_append function)

## Pre-read Tests (patterns)
- tests/unit/layer/*.c (layer test patterns)

## Pre-conditions
- `make check` passes
- Newline conversion in `layer_scrollback.c` appends one byte at a time

## Task
Replace character-by-character newline conversion with batch copy. Current code:

```c
for (size_t j = 0; j < line_len; j++) {
    if (line_text[j] == '\n') {
        ik_output_buffer_append(output, "\r\n", 2);
    } else {
        ik_output_buffer_append(output, &line_text[j], 1);  // 1 byte at a time!
    }
}
```

This should be replaced with batch copying of ranges between newlines:

```c
size_t start = 0;
for (size_t j = 0; j < line_len; j++) {
    if (line_text[j] == '\n') {
        if (j > start) {
            ik_output_buffer_append(output, line_text + start, j - start);
        }
        ik_output_buffer_append(output, "\r\n", 2);
        start = j + 1;
    }
}
if (line_len > start) {
    ik_output_buffer_append(output, line_text + start, line_len - start);
}
```

This reduces function call overhead by ~100x for typical lines.

## TDD Cycle

### Red
1. Create test in `tests/unit/layer/batch_newline_test.c`:
   - Test line with no newlines is copied in one append
   - Test line with single newline produces correct output with \r\n
   - Test line with multiple newlines handles all correctly
   - Test empty line produces empty output
   - Test line ending with newline handles correctly
   - Test line starting with newline handles correctly

2. Run `make check` - tests should pass with current implementation (behavior unchanged)

### Green
1. In `src/layer_scrollback.c`, replace the character-by-character loop with batch copy logic

2. Check `src/layer_input.c` for similar patterns and apply same optimization

3. Run `make check` - expect pass (same behavior, faster execution)

### Refactor
1. Consider extracting batch newline conversion to a helper function if used in multiple places:
   ```c
   void ik_output_buffer_append_with_crlf(ik_output_buffer_t *buf,
                                          const char *text, size_t len);
   ```

2. Verify no memory issues with sanitizers
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Newline conversion uses batch copy instead of byte-by-byte
- Function call overhead significantly reduced
- Same output produced (behavioral equivalence)
