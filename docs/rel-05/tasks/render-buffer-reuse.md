# Task: Pre-allocate Reusable Render Buffers

## Target
Render Performance Optimization - Eliminate per-frame memory allocation

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl_viewport.c (current render frame - lines 208, 244, 275 allocate per frame)
- src/repl.h (ik_repl_ctx_t structure)
- src/layer.c (ik_output_buffer_t structure and operations)

## Pre-read Tests (patterns)
- tests/unit/repl/*.c (existing repl test patterns)
- tests/unit/layer/*.c (layer test patterns)

## Pre-conditions
- `make check` passes
- Framebuffer and output buffer are allocated fresh each frame in `ik_repl_render_frame()`

## Task
Eliminate per-frame memory allocations by pre-allocating reusable render buffers in `ik_repl_ctx_t`. Currently, every keystroke triggers:
1. `talloc_array_` for framebuffer (repl_viewport.c:244)
2. Output buffer allocation (repl_viewport.c:208)
3. `talloc_asprintf_` for cursor escape string (repl_viewport.c:275)

Instead, allocate these once during REPL initialization and reset/reuse on each frame.

## TDD Cycle

### Red
1. Add render buffer fields to `ik_repl_ctx_t` in `src/repl.h`:
   ```c
   // Pre-allocated render buffers (reused each frame)
   char *render_framebuffer;
   size_t render_framebuffer_capacity;
   ik_output_buffer_t *render_output;
   char render_cursor_seq[32];  // Fixed size for cursor positioning
   ```

2. Create test in `tests/unit/repl/render_buffer_test.c`:
   - Test render buffers are allocated during REPL init
   - Test render buffers persist across multiple render calls
   - Test framebuffer grows if needed but is reused
   - Test output buffer is reset (not reallocated) between frames

3. Run `make check` - expect test failures

### Green
1. In `ik_repl_init()` (src/repl_init.c):
   - Allocate initial framebuffer with reasonable capacity (e.g., 16KB)
   - Create persistent output buffer
   - Initialize cursor sequence buffer

2. In `ik_repl_render_frame()` (src/repl_viewport.c):
   - Use `repl->render_framebuffer` instead of allocating new
   - Grow capacity if needed (but don't shrink)
   - Reset `repl->render_output->len = 0` instead of creating new
   - Use `snprintf()` into `repl->render_cursor_seq` instead of `talloc_asprintf_()`

3. Add `ik_output_buffer_reset()` function to `src/layer.c`:
   ```c
   void ik_output_buffer_reset(ik_output_buffer_t *buf)
   {
       buf->len = 0;
   }
   ```

4. Run `make check` - expect pass

### Refactor
1. Remove any dead code paths that allocated per-frame
2. Verify no memory leaks with sanitizers
3. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Render buffers are allocated once in REPL init
- `ik_repl_render_frame()` reuses buffers instead of allocating
- No per-frame `talloc_` calls in the render path
- `ik_output_buffer_reset()` function exists for clearing output buffers
