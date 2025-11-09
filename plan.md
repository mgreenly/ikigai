# REPL Implementation Plan

## Overview

This plan transitions from the vterm-based rendering approach to direct terminal rendering as outlined in `docs/eliminate_vterm.md`. We'll build incrementally with manual verification at each phase.

**Current Status**: Phase 0 complete - all foundation modules built (terminal, input, workspace, cursor, render with vterm). REPL init/cleanup implemented but event loop is a stub.

**Strategy**: Replace vterm immediately (no parallel track), verify with client.c demos at each phase.

**Goal**: Complete REPL with direct rendering, scrollback buffer, and viewport scrolling.

---

## Phase 1: Direct Rendering (Dynamic Zone Only)

**Goal**: Replace vterm rendering with direct terminal rendering for workspace only. Get back to verifiable client.c test immediately.

### Remove Old Rendering Module

**Delete**:
- `src/render.c` (vterm-based implementation)
- `src/render.h` (vterm API)
- `tests/unit/render/render_test.c` (vterm tests)

### New Module: `render_direct`

**Public API** (`src/render_direct.h`):

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
- `calculate_cursor_screen_position()` - UTF-8 aware wrapping (uses utf8proc_charwidth)
- Single framebuffer write to terminal (no per-cell iteration)
- Home cursor + write text + position cursor escape sequence

**Implementation Notes**:
- No caching needed yet (workspace is small, typically < 4KB)
- Full scan on each render is acceptable for this phase
- See `docs/eliminate_vterm.md` lines 183-273 for calculation functions

**Test Coverage**:
- Unit tests: cursor position calculation (wrapping, UTF-8, CJK, emoji)
- Unit tests: rendering with MOCKABLE write wrapper
- OOM injection tests
- 100% coverage required

### Manual Testing via `src/client.c`

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

**Phase 1 Complete When**:
- [ ] Old render module deleted
- [ ] render_direct module implemented with 100% test coverage
- [ ] client.c demo works and passes manual verification
- [ ] `make check && make lint && make coverage` all pass

---

## Phase 2: Complete Basic REPL Event Loop

**Goal**: Full interactive REPL with just workspace (no scrollback). Complete the Phase 1 functionality from the original tasks.md.

### Modifications to `repl` Module

**Update** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_direct_ctx_t *render;      // Changed from ik_render_ctx_t
    ik_workspace_t *workspace;
    ik_input_parser_t *input_parser;
    bool quit;
} ik_repl_ctx_t;
```

**New Functions** in `src/repl.c`:
```c
// Render current frame (workspace only for now)
res_t ik_repl_render_frame(ik_repl_ctx_t *repl);

// Process single input action
res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);

// Event loop (complete implementation)
res_t ik_repl_run(ik_repl_ctx_t *repl);
```

**Action Processing**:
- `IK_INPUT_CHAR` → `ik_workspace_insert_codepoint()`
- `IK_INPUT_NEWLINE` → `ik_workspace_insert_newline()`
- `IK_INPUT_BACKSPACE` → `ik_workspace_backspace()`
- `IK_INPUT_DELETE` → `ik_workspace_delete()`
- `IK_INPUT_ARROW_LEFT/RIGHT` → `ik_workspace_cursor_left/right()`
- `IK_INPUT_ARROW_UP/DOWN` → `ik_workspace_cursor_up/down()` (Task 2.5)
- `IK_INPUT_CTRL_A` → `ik_workspace_cursor_to_line_start()` (Task 2.6)
- `IK_INPUT_CTRL_E` → `ik_workspace_cursor_to_line_end()` (Task 2.6)
- `IK_INPUT_CTRL_K` → `ik_workspace_kill_to_line_end()` (Task 2.6)
- `IK_INPUT_CTRL_U` → `ik_workspace_kill_line()` (Task 2.6)
- `IK_INPUT_CTRL_W` → `ik_workspace_delete_word_backward()` (Task 2.6)
- `IK_INPUT_CTRL_C` → set quit flag

**New Workspace Functions** (Tasks 2.5 & 2.6):
- Multi-line cursor movement: `cursor_up()`, `cursor_down()`
- Line navigation: `cursor_to_line_start()`, `cursor_to_line_end()`
- Line editing: `kill_to_line_end()`, `kill_line()`, `delete_word_backward()`

### Update `src/main.c`

Rename from client.c and implement proper REPL entry point:
```c
int main(void) {
    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(NULL, &repl);
    if (is_err(&result)) { /* handle error */ }

    result = ik_repl_run(repl);
    ik_repl_cleanup(repl);

    return is_ok(&result) ? 0 : 1;
}
```

### Manual Testing via `./ikigai`

**Full REPL Interaction Checklist**:
- [ ] Launch and basic operation
- [ ] UTF-8 handling (emoji, combining chars, CJK)
- [ ] Cursor movement through multi-byte chars (left/right/up/down)
- [ ] Text wrapping at terminal boundary
- [ ] Backspace/delete through wrapped text
- [ ] Insert in middle of wrapped line
- [ ] Multi-line input with newlines
- [ ] Arrow up/down cursor movement in multi-line text
- [ ] Readline shortcuts: Ctrl+A, Ctrl+E, Ctrl+K, Ctrl+U, Ctrl+W
- [ ] Ctrl+C exit and clean terminal restoration

**Phase 2 Complete When**:
- [ ] REPL event loop fully implemented
- [ ] main.c replaces client.c as entry point
- [ ] 100% test coverage maintained
- [ ] Manual testing checklist passes
- [ ] `make check && make lint && make coverage` all pass

---

## Phase 3: Scrollback Buffer Module

**Goal**: Add scrollback buffer storage with layout caching for historical output.

### New Module: `scrollback`

**Public API** (`src/scrollback.h`):

```c
// Layout metadata (hot data for reflow calculations)
typedef struct {
    size_t display_width;    // Pre-computed: sum of all charwidths
    size_t physical_lines;   // Cached: wraps to N lines at cached_width
} ik_line_layout_t;

// Scrollback buffer with separated hot/cold data
typedef struct ik_scrollback_t {
    // Cold data: text content (only accessed during rendering)
    char *text_buffer;           // All line text in one contiguous buffer
    size_t *text_offsets;        // Where each line starts in text_buffer
    size_t *text_lengths;        // Length in bytes of each line

    // Hot data: layout metadata (accessed during reflow and rendering)
    ik_line_layout_t *layouts;   // Parallel array of layout info

    size_t count;                // Number of lines
    size_t capacity;             // Allocated capacity
    int32_t cached_width;        // Terminal width for cached physical_lines
    size_t total_physical_lines; // Cached: sum of all physical_lines
    size_t buffer_used;          // Bytes used in text_buffer
    size_t buffer_capacity;      // Total buffer capacity
} ik_scrollback_t;

// Create scrollback buffer
res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                            ik_scrollback_t **scrollback_out);

// Append line to scrollback (calculates display_width once)
res_t ik_scrollback_append_line(ik_scrollback_t *sb, const char *text, size_t len);

// Ensure layout cache is valid for current terminal width
void ik_scrollback_ensure_layout(ik_scrollback_t *sb, int32_t terminal_width);

// Find which logical line contains a given physical row
size_t ik_scrollback_find_logical_line_at_physical_row(ik_scrollback_t *sb,
                                                        size_t target_row);

// Query functions
size_t ik_scrollback_get_line_count(const ik_scrollback_t *sb);
size_t ik_scrollback_get_total_physical_lines(const ik_scrollback_t *sb);
const char *ik_scrollback_get_line_text(const ik_scrollback_t *sb, size_t index,
                                         size_t *len_out);
```

**Core Logic**:
- `calculate_display_width()` - scan UTF-8 once on line creation
- `calculate_physical_lines()` - O(1) arithmetic: `display_width / terminal_width`
- Cache invalidation on terminal resize
- See `docs/eliminate_vterm.md` lines 105-363 for data structures and algorithms

**Implementation Notes**:
- Pre-compute display_width once per line (never changes)
- Reflow on resize is just arithmetic (no UTF-8 scanning)
- Separated hot/cold data for cache locality
- Performance target: 1000 lines × 50 chars = ~2μs reflow time

**Test Coverage**:
- Unit tests: line append, display_width calculation, physical_lines calculation
- Unit tests: layout cache invalidation on resize
- Unit tests: find logical line at physical row
- OOM injection tests
- 100% coverage required

### Update Workspace Module

**Add to** `src/workspace.h`:
```c
typedef struct ik_workspace_t {
    ik_byte_array_t *text;
    ik_cursor_t *cursor;

    // Layout cache (NEW - for workspace wrapping)
    size_t physical_lines;     // Cached: total wrapped lines
    int32_t cached_width;      // Width this is valid for
    bool layout_dirty;         // Need to recalculate
} ik_workspace_t;

// Ensure workspace layout cache is valid
void ik_workspace_ensure_layout(ik_workspace_t *ws, int32_t terminal_width);

// Invalidate layout cache (call on text edits)
void ik_workspace_invalidate_layout(ik_workspace_t *ws);

// Query cached physical line count
size_t ik_workspace_get_physical_lines(const ik_workspace_t *ws);
```

**Implementation Notes**:
- Workspace needs full scan on recalculation (text is mutable, may contain \n)
- Mark `layout_dirty = true` on any text edit
- Recalculate once per frame before rendering
- See `docs/eliminate_vterm.md` lines 365-413

### Manual Testing via `src/client.c`

**Demo**: Scrollback buffer with manual line additions
- Create scrollback buffer
- Add lines with various content (ASCII, UTF-8, long lines that wrap)
- Query total physical lines before/after wrapping
- Simulate terminal resize, verify reflow speed
- Print some lines back out
- Test with 1000+ lines to verify performance

**Verification Checklist**:
- [ ] Lines stored correctly in contiguous buffer
- [ ] Display width calculated correctly for various UTF-8 content
- [ ] Physical line counts correct after wrapping
- [ ] Terminal resize updates physical_lines correctly
- [ ] Reflow performance acceptable (1000 lines < 5ms)
- [ ] No memory leaks (talloc hierarchy)

**Phase 3 Complete When**:
- [ ] scrollback module implemented with 100% test coverage
- [ ] workspace module extended with layout caching
- [ ] client.c demo works and passes manual verification
- [ ] `make check && make lint && make coverage` all pass

---

## Phase 4: Viewport and Scrolling Integration

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

### Modifications to `repl` Module

**Update** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_direct_ctx_t *render;
    ik_scrollback_t *scrollback;         // NEW - historical output
    ik_workspace_t *workspace;
    ik_input_parser_t *input_parser;
    size_t scroll_offset;                // NEW - viewport scrolling
    bool quit;
} ik_repl_ctx_t;
```

**New Functions** in `src/repl.c`:
```c
// Submit current workspace text to scrollback (e.g., on Ctrl+D or specific trigger)
res_t ik_repl_submit_line(ik_repl_ctx_t *repl);

// Scroll viewport (positive = scroll up, negative = scroll down)
void ik_repl_scroll(ik_repl_ctx_t *repl, int32_t delta);
```

**Modify** `ik_repl_render_frame()`:
- Calculate total content size (scrollback + separator + workspace)
- Calculate viewport window based on scroll_offset
- Render visible portion only
- Use single framebuffer write

**Implementation Notes**:
- Total physical lines = scrollback.total_physical_lines + 1 (separator) + workspace.physical_lines
- Viewport window: show last N physical rows (where N = terminal height)
- Scroll offset adjusts which physical rows are visible

### Modifications to `render_direct` Module

**Update** `src/render_direct.h`:
```c
// Render complete frame: scrollback + separator + workspace
res_t ik_render_direct_frame(ik_render_direct_ctx_t *ctx,
                              ik_scrollback_t *scrollback,
                              ik_workspace_t *workspace,
                              size_t scroll_offset);
```

**Implementation**:
- Build framebuffer with visible scrollback lines + separator + workspace
- Single write to terminal (see `docs/eliminate_vterm.md` lines 418-527)
- Calculate cursor absolute position and viewport position
- Only copy visible text (not entire scrollback)

**Rendering Algorithm**:
1. Ensure all layout caches valid (scrollback + workspace)
2. Calculate viewport window (view_start, view_end)
3. Find which logical lines are visible
4. Build framebuffer: home cursor + visible text + cursor position
5. Single write to terminal

### Add Scrolling Input Actions

**Update** `src/input.h`:
```c
typedef enum {
    // ... existing actions ...
    IK_INPUT_PAGE_UP,      // NEW
    IK_INPUT_PAGE_DOWN,    // NEW
} ik_input_action_type_t;
```

**Update Input Parser**:
- Parse Page Up / Page Down escape sequences
- Return appropriate action types

**Update** `ik_repl_process_action()`:
- `IK_INPUT_PAGE_UP` → scroll viewport up by screen height (scrollback navigation)
- `IK_INPUT_PAGE_DOWN` → scroll viewport down by screen height (scrollback navigation)
- `IK_INPUT_ARROW_UP` → if in workspace editing mode, handled by Phase 2.5; if at viewport boundary, scroll viewport
- `IK_INPUT_ARROW_DOWN` → if in workspace editing mode, handled by Phase 2.5; if at viewport boundary, scroll viewport

**Note**: Arrow up/down have dual purpose:
- **Workspace editing** (Phase 2.5): Move cursor up/down within multi-line workspace
- **Scrollback navigation** (Phase 4): Scroll viewport when cursor is at boundary or in scrollback-only mode

### Manual Testing via `./ikigai`

**Full REPL with Scrollback Checklist**:
- [ ] Submit lines to scrollback (verify trigger mechanism)
- [ ] Scrollback displays historical lines
- [ ] Separator line appears between scrollback and workspace
- [ ] Page Up/Down scrolling works
- [ ] Viewport calculation correct (only visible lines rendered)
- [ ] Cursor stays in correct position during scrolling
- [ ] Terminal resize handles scrollback reflow
- [ ] Performance acceptable with large scrollback (1000+ lines)
- [ ] All Phase 2 tests still work

**Phase 4 Complete When**:
- [ ] Scrollback integrated with REPL
- [ ] Viewport calculation and scrolling implemented
- [ ] render_direct supports full frame rendering
- [ ] 100% test coverage maintained
- [ ] Manual testing checklist passes
- [ ] `make check && make lint && make coverage` all pass

---

## Phase 5: Cleanup and Documentation

**Goal**: Remove vterm dependency from build system, update all documentation, finalize implementation.

### Build System Cleanup

**Update** `Makefile`:
```diff
-CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread -lutf8proc -lvterm
+CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread -lutf8proc
```

**Update Distro Packaging** (remove libvterm-dev dependency):
- `distros/debian/packaging/control`
- `distros/debian/Dockerfile`
- `distros/fedora/packaging/ikigai.spec`
- `distros/fedora/Dockerfile`
- `distros/arch/packaging/PKGBUILD`
- `distros/arch/Dockerfile`

**Verify**:
- [ ] `make distro-check` passes (validate across all distros)
- [ ] Clean builds on Debian, Fedora, Arch

### Documentation Updates

**Update** `docs/architecture.md`:
- Remove libvterm from dependency list
- Document direct rendering approach
- Update rendering section

**Update** `docs/repl/repl-phase-1.md`:
- Document direct rendering implementation
- Update architecture diagrams if present

**Update** `docs/eliminate_vterm.md`:
- Mark as implemented
- Document actual results vs estimates
- Note any deviations from plan

**Update** `README.md` (if needed):
- Update dependency list
- Update build instructions

### Final Verification

**Quality Gates**:
- [ ] `make check` - all tests pass
- [ ] `make lint` - all complexity checks pass
- [ ] `make coverage` - 100% coverage (Lines, Functions, Branches)
- [ ] `make check-dynamic` - all sanitizer checks pass (ASan, UBSan, TSan)
- [ ] `make distro-check` - validate across all distros
- [ ] `make fmt` - format all code

**Final Manual Testing**:
- Run through all checklists from Phases 1-4
- Test on multiple terminal emulators (xterm, gnome-terminal, alacritty, kitty)
- Verify terminal restoration on various exit scenarios (Ctrl+C, error, etc.)

**Phase 5 Complete When**:
- [ ] vterm dependency completely removed from build system
- [ ] All distro packaging updated
- [ ] All documentation updated
- [ ] All quality gates pass
- [ ] Final manual testing complete

**Final Commit**:
- "Eliminate libvterm dependency, implement direct terminal rendering with scrollback"

---

## Module Dependency Graph (Final State)

```
main.c
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render_direct.{c,h}    [NEW - replaces render.c]
       ├─ scrollback.{c,h}        [NEW]
       ├─ workspace.{c,h}         [MODIFIED - add layout caching]
       │    ├─ byte_array.{c,h}
       │    └─ cursor.{c,h}
       └─ input.{c,h}             [MODIFIED - add Page Up/Down]
```

**Dependencies**:
- **Removed**: libvterm
- **Unchanged**: talloc, jansson, uuid, libb64, pthread, libutf8proc

---

## Performance Targets

Based on `docs/eliminate_vterm.md` analysis:

### Scrollback Reflow (Terminal Resize)
- **1000 lines**: < 5ms (target: ~2μs with pre-computed display_width)
- **Operation**: O(n) where n = number of lines, but each line is O(1) arithmetic

### Frame Rendering
- **Write operations**: 1 (entire frame in single write)
- **Bytes processed**: Only visible content (~1,920 chars for 24×80 terminal)
- **vs. vterm**: 52× reduction in syscalls, 26× reduction in bytes processed

### Memory Overhead
- **Per scrollback line**: 32 bytes metadata + text content
- **1000 lines**: ~32 KB metadata (vs ~96 KB for full VTerm grid)
- **Cache locality**: Hot data (layouts) separated from cold data (text)

---

## Development Notes

- **TDD**: Red/Green/Verify cycle for all new code
- **Quality**: 100% test coverage before each phase completion
- **Manual Testing**: Use client.c demos to verify each phase before moving forward
- **Zero Technical Debt**: Fix issues immediately as discovered
- **Incremental**: Each phase builds on the previous, with full verification
- **Strategy**: Replace immediately (Option B) - no parallel vterm/direct tracks
