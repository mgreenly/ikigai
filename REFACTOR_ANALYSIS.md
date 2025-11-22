# Refactor Analysis: OOM-Only Functions

## Summary

Found **18 total** functions returning `res_t` with output parameters:
- **Group A (OOM-only)**: 13 functions → simplify to direct pointer return
- **Group B (Real errors)**: 5 functions → keep `res_t`

---

## Group A: OOM-Only Functions (Simplify These)

These functions only fail on out-of-memory conditions (which PANIC). They should be simplified to return pointers directly.

### 1. `ik_format_buffer_create` - src/format.c:16
**Current**: `res_t ik_format_buffer_create(void *parent, ik_format_buffer_t **buf_out)`
**New**: `ik_format_buffer_t *ik_format_buffer_create(void *parent)`
- Only PANICs on OOM (lines 22, 26)
- Always returns OK(buf)

### 2. `ik_output_buffer_create` - src/layer.c:8
**Current**: `res_t ik_output_buffer_create(TALLOC_CTX *ctx, size_t initial_capacity, ik_output_buffer_t **out)`
**New**: `ik_output_buffer_t *ik_output_buffer_create(TALLOC_CTX *ctx, size_t initial_capacity)`
- Only PANICs on OOM (lines 15, 18)
- Always returns OK(buf)

### 3. `ik_layer_cake_create` - src/layer.c:88
**Current**: `res_t ik_layer_cake_create(TALLOC_CTX *ctx, size_t viewport_height, ik_layer_cake_t **cake_out)`
**New**: `ik_layer_cake_t *ik_layer_cake_create(TALLOC_CTX *ctx, size_t viewport_height)`
- Only PANICs on OOM (lines 94, 98)
- Always returns OK(cake)

### 4. `ik_layer_create` - src/layer.c:59
**Current**: `res_t ik_layer_create(..., ik_layer_t **layer_out)`
**New**: `ik_layer_t *ik_layer_create(...)`
- Only PANICs on OOM (line 75)
- Always returns OK(layer)
- **Note**: This is called by all *_layer_create wrappers

### 5. `ik_scrollback_create` - src/scrollback.c:14
**Current**: `res_t ik_scrollback_create(void *parent, int32_t terminal_width, ik_scrollback_t **scrollback_out)`
**New**: `ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width)`
- Only PANICs on OOM (lines 24, 36, 40, 44, 48)
- Always returns ok(scrollback)

### 6. `ik_input_parser_create` - src/input.c:9
**Current**: `res_t ik_input_parser_create(void *parent, ik_input_parser_t **parser_out)`
**New**: `ik_input_parser_t *ik_input_parser_create(void *parent)`
- Only PANICs on OOM (line 16)
- Always returns OK(parser)

### 7. `ik_input_buffer_cursor_create` - src/input_buffer/cursor.c:15
**Current**: `res_t ik_input_buffer_cursor_create(void *parent, ik_input_buffer_cursor_t **cursor_out)`
**New**: `ik_input_buffer_cursor_t *ik_input_buffer_cursor_create(void *parent)`
- Only PANICs on OOM (line 21)
- Always returns OK(cursor)

### 8. `ik_input_buffer_create` - src/input_buffer/core.c:14
**Current**: `res_t ik_input_buffer_create(void *parent, ik_input_buffer_t **input_buffer_out)`
**New**: `ik_input_buffer_t *ik_input_buffer_create(void *parent)`
- Only PANICs on OOM (lines 19, 22, 26)
- Always returns OK(input_buffer)

### 9. `ik_input_buffer_get_text` - src/input_buffer/core.c:37
**Current**: `res_t ik_input_buffer_get_text(ik_input_buffer_t *input_buffer, char **text_out, size_t *len_out)`
**New**: `const char *ik_input_buffer_get_text(ik_input_buffer_t *input_buffer, size_t *len_out)`
- Never fails - always returns OK(NULL)
- This is a getter, not a creator
- Can return text directly with length via output param

### 10. `ik_separator_layer_create` - src/layer_wrappers.c:49
**Current**: `res_t ik_separator_layer_create(..., ik_layer_t **layer_out)`
**New**: `ik_layer_t *ik_separator_layer_create(...)`
- Only PANICs on OOM (line 61)
- Calls ik_layer_create (also OOM-only)

### 11. `ik_scrollback_layer_create` - src/layer_wrappers.c:167
**Current**: `res_t ik_scrollback_layer_create(..., ik_layer_t **layer_out)`
**New**: `ik_layer_t *ik_scrollback_layer_create(...)`
- Only PANICs on OOM (line 179)
- Calls ik_layer_create (also OOM-only)

### 12. `ik_input_layer_create` - src/layer_wrappers.c:273
**Current**: `res_t ik_input_layer_create(..., ik_layer_t **layer_out)`
**New**: `ik_layer_t *ik_input_layer_create(...)`
- Only PANICs on OOM (line 289)
- Calls ik_layer_create (also OOM-only)

### 13. `ik_spinner_layer_create` - src/layer_wrappers.c:370
**Current**: `res_t ik_spinner_layer_create(..., ik_layer_t **layer_out)`
**New**: `ik_layer_t *ik_spinner_layer_create(...)`
- Only PANICs on OOM (line 382)
- Calls ik_layer_create (also OOM-only)

---

## Group B: Real Error Conditions (KEEP res_t)

These functions have genuine error conditions that callers might handle gracefully.

### 1. `ik_debug_pipe_read` - src/debug_pipe.c:95
**Keep**: `res_t ik_debug_pipe_read(ik_debug_pipe_t *pipe, char ***lines_out, size_t *count_out)`
- Can return ERR for IO errors (line 114: read() failure)
- Real error: EAGAIN/EWOULDBLOCK (non-blocking IO)
- Callers handle errors gracefully

### 2. `ik_mark_find` - src/marks.c:91
**Keep**: `res_t ik_mark_find(ik_repl_ctx_t *repl, const char *label, ik_mark_t **mark_out)`
- Returns ERR when no marks exist (line 98)
- Returns ERR when mark not found (line 116)
- Real validation/search failure errors

### 3. `ik_repl_init` - src/repl.c:24
**Keep**: `res_t ik_repl_init(void *parent, ik_cfg_t *cfg, ik_repl_ctx_t **repl_out)`
- Calls `ik_term_init` which can fail with IO errors
- Calls `ik_render_create` which can fail with validation errors
- Calls `ik_signal_handler_init` which can fail
- Errors are propagated to caller (lines 36-39, 47-49, 145-148)

### 4. `ik_render_create` - src/render.c:14
**Keep**: `res_t ik_render_create(void *parent, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **ctx_out)`
- Returns ERR for invalid dimensions (line 22)
- Real validation error (not just OOM)

### 5. `ik_term_init` - src/terminal.c:14
**Keep**: `res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)`
- Can fail opening /dev/tty (line 22)
- Can fail with tcgetattr (line 34)
- Can fail setting raw mode (line 49)
- Can fail flushing input (line 56)
- Can fail entering alternate screen (line 64)
- Can fail getting terminal size (line 75)
- Many real IO error conditions
- Already mentioned in fix.md as good example

---

## Refactoring Strategy

### Phase 1: Leaf Functions (No Dependencies)
Start with functions that don't call other Group A functions:
1. `ik_input_parser_create`
2. `ik_input_buffer_cursor_create`
3. `ik_format_buffer_create`
4. `ik_output_buffer_create`
5. `ik_scrollback_create`
6. `ik_input_buffer_get_text`

### Phase 2: Intermediate Functions
Then refactor functions that depend on Phase 1:
7. `ik_input_buffer_create` (depends on cursor_create)
8. `ik_layer_cake_create`
9. `ik_layer_create`

### Phase 3: Top-Level Wrappers
Finally, refactor the layer wrapper functions:
10. `ik_separator_layer_create`
11. `ik_scrollback_layer_create`
12. `ik_input_layer_create`
13. `ik_spinner_layer_create`

### Implementation Plan for Each Function
1. Update function signature in header file
2. Update implementation (remove output param, return pointer directly)
3. Find all call sites using grep
4. Update each call site
5. Update corresponding tests
6. Run `make fmt && make check && make lint && make coverage`
7. Verify 100% coverage maintained

---

## Next Steps

1. Start with `ik_format_buffer_create` (already has example in fix.md)
2. Follow the phased approach above
3. Update `docs/return_values.md` with guidance on when NOT to use output params
4. Run full pre-commit checks before final commit
