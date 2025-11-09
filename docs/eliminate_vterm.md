# Eliminate libvterm Dependency

## Executive Summary

**Recommendation**: Remove libvterm dependency and implement direct terminal rendering.

**Rationale**: libvterm provides minimal value in the current architecture. We manage our own text buffers, handle UTF-8/grapheme processing ourselves, and already use alternate screen buffering. The only service vterm provides is calculating cursor screen position after text wrapping - approximately 50-100 lines of logic that we're paying for with a full external dependency.

**Impact**:
- **Code reduction**: Remove 654 lines (render.c + render.h + render_test.c)
- **Simpler implementation**: Replace with ~100-150 lines of direct terminal rendering
- **Performance improvement**: Eliminate double-buffering and cell iteration overhead
- **Reduced complexity**: One fewer dependency to manage across distros
- **Better alignment**: Matches project philosophy of explicit control and minimal dependencies

---

## Current Architecture Analysis

### What We Manage Ourselves

The ikigai codebase already handles all core text operations:

1. **Text Storage**: `ik_byte_array_t` stores UTF-8 text buffer (src/byte_array.c)
2. **Line Management**: `ik_line_array_t` manages multi-line text (src/line_array.c)
3. **Cursor Tracking**: `ik_cursor_t` tracks byte + grapheme offsets (src/cursor.c)
4. **UTF-8 Processing**: libutf8proc for grapheme cluster boundaries (src/cursor.c:14-172)
5. **Input Parsing**: Full UTF-8 decoder with security hardening (src/input.c)
6. **Text Editing**: Insert, delete, backspace operations (src/workspace.c)
7. **Terminal Setup**: Raw mode + alternate screen (src/terminal.c:61-67)

### What libvterm Actually Does

Looking at `src/render.c:121-222`, vterm's role is minimal:

**Composition phase** (`ik_render_write_text`, `ik_render_set_cursor`):
- Stores text in a 2D grid of cells
- Tracks cursor position
- Handles text wrapping to calculate row/col from linear text stream

**Blit phase** (`ik_render_blit`):
- We iterate through **every cell** in the grid (rows × cols)
- We manually encode UTF-8 for each codepoint
- We manually write to terminal file descriptor
- We manually format cursor position escape sequence

**Key observation**: We clear and repopulate the vterm grid on every frame. We're not using it as a persistent buffer - just as a throwaway calculation workspace.

### The Performance Problem

Current approach (src/render.c:159-222):
```c
// 1. Clear screen (unnecessary - alternate screen starts blank)
write(tty_fd, "\x1b[2J\x1b[H", 7);

// 2. Iterate through EVERY cell in terminal (e.g., 24×80 = 1,920 cells)
for (row = 0; row < render->rows; row++) {
    for (col = 0; col < render->cols; col++) {
        vterm_screen_get_cell(vscreen, pos, &cell);
        // Skip empty cells
        if (cell.chars[0] == 0) continue;

        // Manually encode UTF-8 and write
        for each codepoint in cell:
            encode_utf8(codepoint, utf8_buf);
            write(tty_fd, utf8_buf, len);  // Many small writes
    }
}

// 3. Get cursor position from vterm and write escape sequence
```

**Problems**:
- Unnecessary screen clear (alternate screen is already blank)
- 1,920 cell iterations for 24×80 terminal (most empty)
- Many small writes instead of bulk write
- Double buffering (our buffer → vterm grid → terminal)

### The Alternate Screen Advantage

We already use alternate screen buffer (src/terminal.c:61-67):
```c
const char *alt_screen = "\x1b[?1049h";
```

**This gives us automatic double-buffering at the terminal level**:
- Alternate screen starts blank
- We can overwrite it completely on each frame
- Terminal handles the actual display buffering
- No need for vterm's virtual terminal layer

---

## Proposed Solution

### Direct Terminal Rendering

Replace the render module with direct writes:

```c
res_t ik_render_blit_direct(const char *text, size_t text_len,
                            size_t cursor_byte_offset,
                            int terminal_width, int terminal_height,
                            int tty_fd)
{
    // 1. Go to home position (cursor to 0,0)
    write(tty_fd, "\x1b[H", 3);

    // 2. Write text directly (terminal auto-wraps)
    write(tty_fd, text, text_len);

    // 3. Calculate cursor screen position
    int cursor_row, cursor_col;
    calculate_cursor_position(text, cursor_byte_offset,
                             terminal_width,
                             &cursor_row, &cursor_col);

    // 4. Position cursor
    char seq[32];
    snprintf(seq, sizeof(seq), "\x1b[%d;%dH",
             cursor_row + 1, cursor_col + 1);
    write(tty_fd, seq, strlen(seq));

    return OK(NULL);
}
```

**Total writes**: 3 (home, text, cursor) vs. potentially hundreds

### Cursor Position Calculation

The only non-trivial logic vterm provides. **Note**: We already have partial implementation - the cursor module (src/cursor.c) tracks byte and grapheme offsets correctly through UTF-8 text. We just need to extend this to calculate screen row/col position accounting for wrapping.

```c
// Calculate where cursor appears on screen after text wrapping
static res_t calculate_cursor_position(const char *text,
                                       size_t cursor_byte_offset,
                                       int terminal_width,
                                       int *row_out, int *col_out)
{
    int row = 0;
    int col = 0;
    size_t pos = 0;

    // Iterate through text up to cursor position
    while (pos < cursor_byte_offset && pos < text_len) {
        // Handle newlines
        if (text[pos] == '\n') {
            row++;
            col = 0;
            pos++;
            continue;
        }

        // Decode UTF-8 codepoint (we already have this logic)
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes = utf8proc_iterate(...);

        if (bytes <= 0) {
            return ERR(..., INVALID_ARG, "Invalid UTF-8");
        }

        // Get display width (accounts for wide chars like CJK)
        int width = utf8proc_charwidth(codepoint);

        // Check for line wrap
        if (col + width > terminal_width) {
            row++;
            col = 0;
        }

        col += width;
        pos += bytes;
    }

    *row_out = row;
    *col_out = col;
    return OK(NULL);
}
```

**Estimated size**: 50-80 lines with proper error handling

**Note**: We already use libutf8proc, which provides `utf8proc_charwidth()` for display width calculation. This handles:
- Single-width ASCII: width = 1
- Wide characters (CJK): width = 2
- Zero-width combining chars: width = 0
- Emoji: width = 1 or 2 depending on codepoint

---

## Impact Analysis

### Code Changes Required

**Files to remove**:
- `src/render.c` (222 lines)
- `src/render.h` (86 lines)
- `tests/unit/render/render_test.c` (346 lines)
- **Total removal**: 654 lines

**Files to create**:
- `src/render_direct.c` (~100-120 lines)
- `src/render_direct.h` (~30-40 lines)
- `tests/unit/render_direct/render_direct_test.c` (~150-200 lines)
- **Total addition**: ~280-360 lines

**Net reduction**: ~300-400 lines

**Files to modify**:
- `src/repl.h`: Update render context type
- `src/repl.c`: Use new render API
- `Makefile`: Remove `-lvterm` from CLIENT_LIBS (line 71)
- All distro packaging files: Remove libvterm-dev dependency

### Build System Impact

**Makefile changes**:
```diff
-CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread -lutf8proc -lvterm
+CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread -lutf8proc
```

**Distro packaging** (8 files to update):
- `distros/debian/packaging/control`: Remove libvterm-dev
- `distros/debian/Dockerfile`: Remove libvterm-dev
- `distros/fedora/packaging/ikigai.spec`: Remove libvterm-devel
- `distros/fedora/Dockerfile`: Remove libvterm-devel
- `distros/arch/packaging/PKGBUILD`: Remove libvterm
- `distros/arch/Dockerfile`: Remove libvterm

### Dependency Impact

**Current dependencies**:
- talloc (memory management)
- jansson (JSON parsing)
- uuid (identifier generation)
- libb64 (base64 encoding)
- pthread (threading)
- libutf8proc (UTF-8/Unicode processing)
- **libvterm** ← Remove this

**Remaining dependencies after change**: 6 (down from 7)

**Note**: All remaining dependencies are actively maintained except libb64 (see docs/vulnerabilities.md). libvterm removal reduces distro maintenance burden.

### Testing Impact

**Current test coverage**:
- `tests/unit/render/render_test.c`: 346 lines, 100% coverage

**New test requirements**:
- Test cursor position calculation for:
  - Simple text (no wrapping)
  - Text with newlines
  - Text that wraps at terminal boundary
  - Wide characters (CJK, emoji)
  - Combining characters
  - Mixed content
- Test rendering edge cases:
  - Empty text
  - Cursor at start/middle/end
  - Terminal width changes
- OOM injection tests (via MOCKABLE wrappers)

**Coverage requirement**: Maintain 100% (lines, functions, branches)

---

## Performance Comparison

### Current (with vterm)

**Per frame render**:
1. Clear render context: O(1) vterm call
2. Write text to vterm: O(n) where n = text length
3. Set cursor in vterm: O(1) vterm call
4. Blit to terminal:
   - Clear screen: 1 write (7 bytes)
   - Iterate all cells: O(rows × cols) = O(1,920) for 24×80
   - Write non-empty cells: Many small writes
   - Write cursor: 1 write (~10 bytes)

**Total complexity**: O(text_length + rows × cols)

**Write operations**: 1 clear + N cell writes + 1 cursor (N can be hundreds)

### Proposed (direct)

**Per frame render**:
1. Calculate cursor position: O(cursor_byte_offset)
2. Write to terminal:
   - Home cursor: 1 write (3 bytes)
   - Write text: 1 write (text_length bytes)
   - Position cursor: 1 write (~10 bytes)

**Total complexity**: O(cursor_byte_offset) ≈ O(text_length) in worst case

**Write operations**: Exactly 3 (home, text, cursor)

### Benchmark Estimate

For typical REPL usage (100 bytes text, 24×80 terminal, cursor in middle):

**Current**:
- Text write to vterm: ~100 byte copies
- Cell iteration: 1,920 iterations (most empty, but still iterated)
- Terminal writes: 1 clear + ~50 cell writes + 1 cursor = ~52 writes

**Proposed**:
- Cursor calculation: ~50 byte scans (up to cursor position)
- Terminal writes: 3 writes (home + text + cursor)

**Expected speedup**: ~10-20x fewer write syscalls, ~40x fewer iterations

---

## Risk Analysis

### Risks

1. **Cursor position bugs**: Most complex part of new code
   - **Mitigation**: Comprehensive unit tests, manual testing checklist
   - **Fallback**: Can reference vterm code if needed during development

2. **Wide character handling**: CJK, emoji display width calculation
   - **Mitigation**: libutf8proc provides `utf8proc_charwidth()`
   - **Testing**: Manual testing checklist includes emoji, CJK characters

3. **Terminal compatibility**: Different terminals may handle wrapping differently
   - **Mitigation**: Standard ANSI behavior is well-defined
   - **Testing**: Test on multiple terminal emulators (xterm, gnome-terminal, alacritty)

4. **Regression risk**: Breaking existing functionality
   - **Mitigation**: 100% test coverage requirement, manual testing checklist
   - **Quality gates**: `make ci` must pass (lint, coverage, dynamic analysis)

### Benefits

1. **Fewer dependencies**: Easier to build and package
2. **Better performance**: Fewer writes, less iteration
3. **Simpler code**: Direct control, no abstraction layer
4. **Alignment with philosophy**: Explicit control, minimal dependencies
5. **Reduced maintenance**: One fewer library to track for CVEs
6. **Better testability**: Direct logic easier to test than vterm integration
7. **Simplified testing**: No need to mock vterm or test vterm integration - pure logic tests
8. **Leverages existing work**: Cursor module already handles UTF-8/grapheme tracking correctly

---

## Implementation Plan

### Phase 1: Implement Direct Rendering (Parallel Track)

**Goal**: Build new render_direct module alongside existing render module

1. Create `src/render_direct.h` and `src/render_direct.c`
2. Implement cursor position calculation with `utf8proc_charwidth()`
3. Implement direct blit function
4. Write comprehensive unit tests
5. Achieve 100% coverage

**Quality gates**: `make check && make lint && make coverage`

**Files**: Keep old render module untouched during development

### Phase 2: Integration and Testing

**Goal**: Switch REPL to use new render_direct module

1. Modify `src/repl.c` to use `ik_render_direct_*` APIs
2. Update integration tests
3. Comprehensive manual testing (see tasks.md:136-165)
4. Verify clean terminal restoration
5. Test on multiple terminal emulators

**Quality gates**: `make check && make ci`

### Phase 3: Cleanup and Dependency Removal

**Goal**: Remove old code and dependency

1. Remove `src/render.c`, `src/render.h`, `tests/unit/render/`
2. Update `Makefile`: Remove `-lvterm` from CLIENT_LIBS
3. Update all 6 distro packaging files (remove libvterm)
4. Update `docs/architecture.md`: Remove libvterm from dependency list
5. Run `make distro-check` to validate across all distros
6. Run `make ci` to validate complete test suite

**Quality gates**: `make distro-check && make ci`

### Phase 4: Documentation

**Goal**: Update documentation

1. Update `docs/repl/repl-phase-1.md`: Document direct rendering approach
2. Update `docs/architecture.md`: Remove libvterm, update rendering description
3. Update this document with actual results
4. Commit with message: "Eliminate libvterm dependency, implement direct terminal rendering"

---

## Testing Strategy

### Unit Tests

**Cursor position calculation**:
- Simple ASCII text (no wrapping)
- Text with newlines
- Text wrapping at terminal boundary
- Wide characters (CJK): 2-cell width
- Emoji with modifiers
- Combining characters: 0-cell width
- Mixed content (ASCII + wide + combining)
- Cursor at start, middle, end
- Edge cases: empty text, terminal width = 1

**Direct blit**:
- Normal text rendering
- Empty text
- Text longer than screen
- Invalid file descriptor
- OOM scenarios (via MOCKABLE wrappers)

**Coverage requirement**: 100% (lines, functions, branches)

### Integration Tests

- REPL event loop with direct rendering
- Multiple frames rendered in sequence
- Terminal restoration on exit
- Error handling paths

### Manual Testing Checklist

From tasks.md:136-165, verify:

- [ ] Launch and basic operation
- [ ] UTF-8 handling: emoji, combining chars, CJK
- [ ] Cursor movement through multi-byte chars
- [ ] Text wrapping at terminal boundary
- [ ] Backspace/delete through wrapped text
- [ ] Insert in middle of wrapped line
- [ ] Terminal resize (if we handle SIGWINCH)
- [ ] Ctrl+C exit and terminal restoration
- [ ] Test on multiple terminal emulators:
  - [ ] xterm
  - [ ] gnome-terminal
  - [ ] alacritty
  - [ ] kitty

---

## Decision

**Recommendation**: Proceed with elimination of libvterm.

**Justification**:
1. **Minimal value**: vterm only provides ~50 lines worth of cursor calculation
2. **Performance**: Direct rendering is significantly faster
3. **Simplicity**: Fewer layers of abstraction
4. **Dependencies**: Reduces maintenance burden
5. **Philosophy alignment**: Matches project's minimal dependency approach
6. **Low risk**: Comprehensive testing strategy, quality gates enforced

**Timeline**: Can be done in current Phase 1 work before moving to Phase 2 (scrollback). Alternative: Defer to between Phase 1 and Phase 2.

**Blocker check**: No blockers. All necessary functionality (utf8proc_charwidth) already available via libutf8proc.

---

## Appendix: Code Size Analysis

**Current render module**:
- `src/render.h`: 86 lines
- `src/render.c`: 222 lines
- `tests/unit/render/render_test.c`: 346 lines
- **Total**: 654 lines

**Key functions in render.c**:
- `encode_utf8()`: 26 lines (manual UTF-8 encoding)
- `ik_render_create()`: 45 lines (vterm initialization)
- `ik_render_clear()`: 8 lines (vterm reset)
- `ik_render_write_text()`: 18 lines (vterm write wrapper)
- `ik_render_set_cursor()`: 18 lines (vterm cursor wrapper)
- `ik_render_blit()`: 64 lines (cell iteration + terminal writes)

**Note**: We manually encode UTF-8 in render.c despite vterm handling it, because we need to convert vterm's codepoint cells back to UTF-8 for the terminal.

**Estimated new module**:
- `src/render_direct.h`: ~35 lines (simpler API)
- `src/render_direct.c`: ~100 lines (cursor calc + blit)
- `tests/unit/render_direct/render_direct_test.c`: ~200 lines
- **Total**: ~335 lines

**Net savings**: ~320 lines + simpler logic + better performance
