# Eliminate libvterm: Architecture Analysis

This document analyzes the current architecture and evaluates the impact of removing libvterm.

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

**Memory**: Full VTerm grid (24 rows × 80 cols × ~50 bytes/cell = ~96 KB per screen)

### Proposed (direct with caching)

**Per frame render**:
1. Ensure layout cache valid:
   - Scrollback: O(1) if cached, O(n×m) on resize (n=lines, m=avg length)
   - Workspace: O(1) if cached, O(m) on edit (m=workspace length)
2. Calculate viewport: O(n) iterate scrollback lines (just summing cached values)
3. Build frame buffer: O(k) where k = visible text
4. Calculate cursor position: O(cursor_byte_offset) - typically < 1KB
5. Write to terminal: 1 write (entire frame)

**Total complexity**:
- Normal frame: O(n + cursor_offset) where n = number of scrollback lines
- After resize: O(n×m) where m = average line length (rare)
- After edit: O(workspace_length) (once per frame, not per keystroke)

**Write operations**: Exactly 1 (entire frame in single write)

**Memory**:
- Cache overhead: ~16 bytes per scrollback line + ~16 bytes per buffer
- Frame buffer: ~64 KB temporary (freed after write)
- VTerm: 0 bytes (eliminated)

### Benchmark Estimate

For typical REPL usage:
- 1000 scrollback lines, 50 chars average
- 256 bytes workspace text (typical), up to 4K max
- 24 rows × 80 cols terminal
- Cursor in middle of workspace

**Current (with vterm)**:
- Text write to vterm: ~50,100 byte copies
- Cell iteration: 1,920 cell iterations
- UTF-8 encoding per cell: ~50-100 encode operations
- Terminal writes: 1 clear + ~50 cell writes + 1 cursor = ~52 writes
- Per frame cost: ~52 syscalls + 1,920 iterations + 50,100 bytes processed

**Proposed (direct with caching)**:
- Layout cache check: O(1) if valid (typical case)
- Workspace recalc (if edited): scan 256 bytes typical (up to 4K max)
- Viewport calculation: sum 1000 cached values
- Cursor calculation: scan 128 bytes (up to cursor, typical)
- Frame buffer build: copy visible text only (~1,920 chars)
- Terminal writes: 1 write
- Per frame cost: 1 syscall + ~384 bytes scanned typical + ~1,920 bytes copied

**Comparison**:
- Write syscalls: **52× reduction** (52 → 1)
- Bytes processed per frame: **26× reduction** (50,100 → 1,920 typical)
- Memory overhead: **Eliminated 96 KB VTerm grid**, added ~16 KB cache
- No cell iteration overhead
- No UTF-8 encoding/decoding overhead

### Terminal Resize Performance

**Current (with vterm)**:
- Create new VTerm with new dimensions: O(rows × cols)
- Write all text to new VTerm: O(text_length)
- Cost per resize: ~O(rows × cols + text_length)

**Proposed (direct with pre-computed display_width)**:
- Recalculate all scrollback physical lines: O(n) where n=lines
- Each line: O(1) arithmetic (`display_width / terminal_width`)
- Recalculate workspace physical lines: O(workspace_length) - full scan needed
- Cost per resize: ~O(1000) for 1000 lines + O(workspace_length)

**Resize time estimate**:
- 1000 scrollback lines × 2ns (integer division) = 2μs
- Workspace typical: 256 bytes × 50ns (UTF-8 decode + charwidth) = 13μs
- Workspace worst case: 4K × 50ns = 205μs
- **Total typical: ~15 microseconds** (imperceptible)
- **Total worst case: ~207 microseconds** (still imperceptible)
- Compare to old approach: ~2.5 milliseconds
- **160× faster (typical), 12× faster (worst case)**

### Gap Buffer Consideration

**Question**: Should we use a gap buffer for the edit zone to avoid `memmove` on every keystroke?

**Answer**: Not needed.

**Analysis**:
- Current approach: `ik_byte_array_t` uses `memmove` to shift bytes on insert
- Worst case: Insert at position 2048 in 4K buffer = shift 2048 bytes
- Cost: ~0.5μs per keystroke (memmove is ~4GB/s)
- At 60 FPS: 0.5μs per 16ms frame = 0.003% of frame budget

**Gap buffer trade-offs**:
- ✅ Eliminates memmove for sequential inserts at cursor
- ❌ Requires gap relocation when cursor moves (same memmove cost, just delayed)
- ❌ More complex implementation and state management
- ❌ Adds ~100-200 lines of code vs current simple array

**Decision**: Keep simple byte array. The ~1μs per keystroke is not a bottleneck. Profile first, optimize if measurements show actual performance issues.

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

