# REPL Terminal - Phase 1: Direct Rendering

[← Back to REPL Terminal Overview](README.md)

**Goal**: Implement direct terminal rendering for workspace without external terminal emulator library.

**Status**: ✅ COMPLETE

## Rationale

libvterm provided minimal value - we manage our own text buffers, handle UTF-8/grapheme processing ourselves, and already use alternate screen buffering. The only service vterm provided was calculating cursor screen position after text wrapping - approximately 50-100 lines of logic that we were paying for with a full external dependency.

See archived design docs in `docs/archive/eliminate-vterm-*.md` for the complete analysis that led to this decision.

**Benefits**:
- **Simpler**: ~100-150 lines of direct rendering vs 654 lines of vterm integration
- **Faster**: Single write syscall vs 52 writes, 26× fewer bytes processed per frame
- **Fewer dependencies**: One less library to maintain across distros
- **Better performance**: Eliminate double-buffering and cell iteration overhead

## Implementation Tasks

### Task 1: Remove Old Rendering Module

**Delete**:
- `src/render.c` (vterm-based implementation, 222 lines)
- `src/render.h` (vterm API, 86 lines)
- `tests/unit/render/render_test.c` (vterm tests, 346 lines)
- **Total removal**: 654 lines

### Task 2: Implement Direct Rendering Module

**Create**: `src/render_direct.h` and `src/render_direct.c`

**Public API**:
```c
typedef struct ik_render_direct_ctx_t {
    int32_t rows;      // Terminal height
    int32_t cols;      // Terminal width
    int32_t tty_fd;    // Terminal file descriptor
} ik_render_direct_ctx_t;

// Create render context
res_t ik_render_direct_create(void *parent, int32_t rows, int32_t cols,
                               int32_t tty_fd, ik_render_direct_ctx_t **ctx_out);

// Render workspace to terminal (text + cursor positioning)
res_t ik_render_direct_workspace(ik_render_direct_ctx_t *ctx,
                                  const char *text, size_t text_len,
                                  size_t cursor_byte_offset);
```

**Core Logic**:
- `calculate_cursor_screen_position()` - UTF-8 aware wrapping using `utf8proc_charwidth()`
- Single framebuffer write to terminal (no per-cell iteration)
- Home cursor + write text + position cursor escape sequence

**Implementation Notes**:
- No caching needed yet (workspace is small, typically < 4KB)
- Full scan on each render is acceptable for this phase
- UTF-8 aware cursor positioning using `utf8proc_charwidth()`

**Estimated size**: ~100-120 lines (vs 222 lines for vterm integration)

### Task 3: Comprehensive Unit Tests

**Test Coverage** (`tests/unit/render_direct/render_direct_test.c`):
- Cursor position calculation:
  - Simple ASCII text (no wrapping)
  - Text with newlines
  - Text wrapping at terminal boundary
  - Wide characters (CJK): 2-cell width
  - Emoji with modifiers
  - Combining characters: 0-cell width
  - Mixed content (ASCII + wide + combining)
  - Cursor at start, middle, end
  - Edge cases: empty text, terminal width = 1
- Rendering:
  - Normal text rendering
  - Empty text
  - Text longer than screen
  - Invalid file descriptor
  - OOM scenarios (via MOCKABLE wrappers)

**Coverage requirement**: 100% (lines, functions, branches)

**Estimated size**: ~150-200 lines

### Task 4: Update REPL Module

**Modify** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_direct_ctx_t *render;      // Changed from ik_render_ctx_t
    ik_workspace_t *workspace;
    ik_input_parser_t *input_parser;
    bool quit;
} ik_repl_ctx_t;
```

**Update** `src/repl.c`:
- Change render context creation to use `ik_render_direct_create()`
- Update any render API calls (currently none - event loop is a stub)

### Task 5: Manual Verification via client.c

**Demo**: Simple text editor using render_direct
- Initialize terminal + render_direct context
- Simple loop: read char → append to buffer → render
- Test: typing, cursor positioning, wrapping
- Exit: Ctrl+C, verify clean terminal restoration

**Verification Checklist**:
- [ ] Text displays correctly
- [ ] Cursor appears at correct position
- [ ] Text wraps at terminal boundary
- [ ] UTF-8 characters (emoji, CJK) display properly
- [ ] Terminal restores cleanly on exit

## What We Validate

- Direct terminal rendering without vterm
- UTF-8 aware cursor position calculation
- Character display width handling (CJK, emoji, combining chars)
- Text wrapping at terminal boundary
- Single-write framebuffer approach (no flicker)
- Clean terminal restoration on exit

## What We Defer

- Layout caching (comes in Phase 3 for scrollback)
- Scrollback rendering (comes in Phase 4)
- Viewport calculation (comes in Phase 4)
- Full REPL event loop (comes in Phase 2)

## Phase 1 Complete When

- [ ] Old render module deleted
- [ ] render_direct module implemented with 100% test coverage
- [ ] REPL module updated to use render_direct
- [ ] client.c demo works and passes manual verification
- [ ] `make check && make lint && make coverage` all pass

## Development Approach

Strict TDD with 100% coverage requirement.
