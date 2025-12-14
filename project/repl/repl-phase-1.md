# REPL Terminal - Phase 1: Direct Rendering

[← Back to REPL Terminal Overview](README.md)

**Goal**: Implement direct terminal rendering for input buffer without external terminal emulator library.

## Approach

Direct ANSI terminal rendering with single-write framebuffer approach.

**Benefits**:
- **Simpler**: ~100-150 lines of direct rendering
- **Faster**: Single write syscall, minimal bytes per frame
- **No external dependencies**: Direct ANSI escape sequences
- **Better performance**: No double-buffering overhead

## Implementation

### Direct Rendering Module

**Files**: `src/render.h` and `src/render.c`

**Public API**:
```c
typedef struct ik_render_ctx_t {
    int32_t rows;      // Terminal height
    int32_t cols;      // Terminal width
    int32_t tty_fd;    // Terminal file descriptor
} ik_render_ctx_t;

// Create render context
res_t ik_render_create(void *parent, int32_t rows, int32_t cols,
                       int32_t tty_fd, ik_render_ctx_t **ctx_out);

// Render input buffer to terminal (text + cursor positioning)
res_t ik_render_input_buffer(ik_render_ctx_t *ctx,
                             const char *text, size_t text_len,
                             size_t cursor_byte_offset);
```

**Core Logic**:
- `calculate_cursor_screen_position()` - UTF-8 aware wrapping using `utf8proc_charwidth()`
- Single framebuffer write to terminal (no per-cell iteration)
- Home cursor + write text + position cursor escape sequence

**Implementation Notes**:
- No caching needed for input buffer (typically < 4KB)
- Full scan on each render is acceptable
- UTF-8 aware cursor positioning using `utf8proc_charwidth()`

### Test Coverage

**Test file**: `tests/unit/render/render_test.c`

**Coverage areas**:
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

### REPL Module Update

**Modified** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_ctx_t *render;      // Direct rendering context
    ik_input_buffer_t *input_buffer;
    ik_input_parser_t *input_parser;
    bool quit;
} ik_repl_ctx_t;
```

**Updated** `src/repl.c`:
- Changed render context creation to use `ik_render_create()`
- Updated render API calls for event loop

### Manual Verification

Simple text editor demo using render module verified:
- Text displays correctly
- Cursor appears at correct position
- Text wraps at terminal boundary
- UTF-8 characters (emoji, CJK) display properly
- Terminal restores cleanly on exit

## What Was Validated

- Direct terminal rendering with ANSI escape sequences
- UTF-8 aware cursor position calculation
- Character display width handling (CJK, emoji, combining chars)
- Text wrapping at terminal boundary
- Single-write framebuffer approach (no flicker)
- Clean terminal restoration on exit

## Performance Results

- **52× syscall reduction** compared to per-cell updates
- **26× byte reduction** in terminal writes
- Single framebuffer write per render cycle
