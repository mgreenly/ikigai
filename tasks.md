# Phase 2.5 - Remove Server and Protocol Code

**Status**: COMPLETE (2025-11-13)
**Goal**: Remove all server and protocol-related code before Phase 3. This eliminates maintenance burden and jansson complexity since v1.0 is a desktop client only.

## Rationale

v1.0 is a **desktop client** that connects directly to LLM APIs (OpenAI, Anthropic, etc.). The server/protocol code was from an earlier architecture exploration and is no longer needed.

**Benefits of removal**:
- Eliminates jansson dependency (reference counting complexity)
- Removes ~700 lines of unused code (src + tests)
- Simplifies build system (no ikigai-server target)
- Reduces maintenance burden before Phase 3+
- Cleaner architecture documentation

**Scope**:
- Remove server binary target (`src/server.c`)
- Remove protocol module (`src/protocol.{c,h}`)
- Remove protocol tests (unit + integration)
- Remove jansson from config.c (convert to simpler parser or defer)
- Update Makefile to remove server targets
- Update documentation to reflect desktop-only architecture

---

## Task 1: Remove Protocol Module

### 1.1: Delete Protocol Source Files
- [x] Delete: `src/protocol.c` (294 lines) - already removed
- [x] Delete: `src/protocol.h` (38 lines) - already removed
- [x] Remove from Makefile: `MODULE_SOURCES` reference to `src/protocol.c`

### 1.2: Delete Protocol Tests
- [x] Delete: `tests/unit/protocol/` directory (4 test files) - already removed
- [x] Delete: `tests/integration/protocol_integration_test.c` - already removed
- [x] Remove from Makefile: All protocol test targets and build rules

### 1.3: Quality Check
- [x] Build: `make clean && make build/ikigai`
- [x] Verify: Client builds successfully without protocol module
- [x] Run: `make check` (all remaining tests pass)

---

## Task 2: Remove Server Binary

### 2.1: Delete Server Source
- [x] Delete: `src/server.c` (41 lines)
- [x] Remove from Makefile: `SERVER_SOURCES`, `SERVER_TARGET`, server build rules
- [x] Remove from Makefile: Install/uninstall rules for ikigai-server

### 2.2: Update Build System
- [x] Update Makefile `all` target: Remove ikigai-server
- [x] Update Makefile `clean` target: Remove server artifacts
- [x] Update Makefile help text: Remove server references
- [x] Verify: `make all` builds only client binary

### 2.3: Quality Check
- [x] Build: `make clean && make all`
- [x] Verify: Only `bin/ikigai` is built (no ikigai-server)
- [x] Run: `make check && make lint`

---

## Task 3: Remove or Simplify Config Jansson Usage

**Decision Required**: Config currently uses jansson for JSON parsing. Options:

**Option A: Keep jansson temporarily (minimal change)**
- Keep jansson in config.c only
- Keep wrapper.c test seam for `json_is_object()`
- Document that jansson is config-only until LLM integration
- Defer JSON library decision to Phase 3 (OpenAI integration)

**Option B: Convert config to simpler format (more work)**
- Replace JSON with key=value format or TOML
- Remove jansson dependency entirely
- Update config tests

**Recommendation**: **Option A** (keep jansson in config.c for now)
- Simpler: Removes protocol/server code (~700 lines) without config rewrite
- Flexible: Revisit JSON library during Phase 3 when we add OpenAI API
- Safe: Config is well-tested, no need to change working code

### 3.1: Document Jansson Scope (Option A)
- [x] Add comment to `src/config.c`: "Jansson used for config parsing only. Will revisit JSON library during Phase 3 (OpenAI integration)."
- [x] Keep `src/wrapper.{c,h}` test seam for `json_is_object()`
- [x] Keep jansson in Makefile dependencies
- [x] Update `docs/architecture.md`: Document jansson scope (config only)

### 3.2: Verify Config Still Works
- [x] Build: `make clean && make build/ikigai`
- [x] Run: Config tests pass
- [x] Manual: Verify config loading works with test file

---

## Task 4: Update Documentation

### 4.1: Update Architecture Docs
- [x] Update `docs/architecture.md`: No changes needed (already desktop-only)
- [x] Update `docs/README.md`: Already documents Phase 2.5
- [x] Condense `docs/jansson_to_yyjson_proposal.md`: 1331 → 213 lines
- [x] Condense `docs/memory_usage_analysis.md`: 573 → 255 lines

### 4.2: Update Build Documentation
- [x] No changes needed - build docs accurate

### 4.3: Archive Old Design Docs
- [x] Not needed - server/protocol docs already historical

---

## Task 5: Verification and Commit

### 5.1: Final Quality Gates
- [x] Run: `make clean`
- [x] Run: `make all` (builds only ikigai client)
- [x] Run: `make check` (all tests pass)
- [x] Run: `make lint` (complexity checks pass)
- [x] Run: `make coverage` (100% coverage maintained)
- [x] Run: `make check-dynamic` (not run, but not required for commit)
- [x] Manual: `./bin/ikigai` launches and works

### 5.2: Code Statistics
- [x] Count lines removed:
  - `src/protocol.{c,h}` (already removed earlier)
  - `src/server.c` (41 lines)
  - Protocol tests (already removed earlier)
  - Total: 1,483 net lines removed (1,775 deletions - 292 insertions)
- [x] Verify Makefile cleanup (server targets removed)
- [x] Verify documentation updated

### 5.3: Create Commit
- [x] Git add deletions and documentation updates
- [x] Commit created: 3169dc3

### 5.4: Verify Post-Commit
- [x] Run: `make clean && make all`
- [x] Run: `make check && make lint && make coverage`
- [x] Verify: All quality gates pass
- [x] Ready for Phase 3

---

## Phase 2.5 Completion Checklist

- [x] Protocol module removed (src + tests)
- [x] Server binary removed (src + Makefile targets)
- [x] Jansson scope documented (config only)
- [x] Documentation condensed (jansson proposal, memory analysis)
- [x] All quality gates pass (check, lint, coverage)
- [x] Commit created: 3169dc3
- [x] 1,483 net lines removed (1,775 deletions - 292 insertions)
- [x] Ready to proceed to Phase 2.75 (PP infrastructure)

---

# Phase 2.75 - Pretty-Print (PP) Infrastructure

**Status**: INFRASTRUCTURE COMPLETE - REPL Integration DEFERRED to Phase 3 (2025-11-13)
**Goal**: Build debug pretty-printing infrastructure for inspecting internal C data structures.

## Rationale

PP infrastructure enables debugging and inspection capabilities. The format module and PP functions are ready, but REPL integration is deferred to Phase 3 because:
- **Design Principle**: All visible output MUST go through screenbuffer → blit to alternate buffer (NEVER stdout/stderr)
- Without scrollback buffer, cannot display PP output properly

## Task 1: Format Buffer Module ✅ COMPLETE

**Create**: `src/format.h` and `src/format.c`

### 1.1: Implement Format Buffer
- [x] Created format.h with public API (6 functions)
- [x] Created format.c (146 lines) - printf-style formatting into byte buffers
- [x] Wraps `ik_byte_array_t` for text accumulation
- [x] `ik_format_appendf()` uses `vsnprintf()` internally
- [x] Null-terminates buffer automatically
- [x] All memory managed via talloc hierarchy

### 1.2: Comprehensive Tests
- [x] Created `tests/unit/format/format_basic_test.c` (362 lines, 15 tests)
- [x] Created `tests/unit/format/format_oom_test.c` (219 lines, 7 tests)
- [x] Tests cover: append, appendf, indent, get_string, get_length
- [x] OOM injection via MOCKABLE wrappers
- [x] 100% coverage: 72/72 lines, 6/6 functions, 28/28 branches

## Task 2: Generic PP Helpers ✅ COMPLETE

**Create**: `src/pp_helpers.h` and `src/pp_helpers.c`

### 2.1: Implement Generic Formatters
- [x] Created pp_helpers.h with reusable formatting functions
- [x] Created pp_helpers.c (3080 lines) with implementations:
  - `ik_pp_header()` - Print type header with address
  - `ik_pp_pointer()` - Print named pointer field
  - `ik_pp_size_t()` - Print named size_t field
  - `ik_pp_int32()` - Print named int32_t field
  - `ik_pp_uint32()` - Print named uint32_t field
  - `ik_pp_string()` - Print named string field (with escaping)
  - `ik_pp_bool()` - Print named boolean field
- [x] All helpers respect indent parameter for proper nesting

### 2.2: Tests
- [x] Comprehensive tests for all generic helper functions
- [x] 100% coverage maintained

## Task 3: PP Functions for Core Data Structures ✅ COMPLETE

**Create**: `src/workspace_pp.c` and `src/workspace_cursor_pp.c`

### 3.1: Implement ik_pp_workspace()
- [x] Created `src/workspace_pp.c` (91 lines)
- [x] Shows workspace address, text length, cursor positions, target_column
- [x] Uses generic helpers from pp_helpers.c
- [x] Calls `ik_pp_cursor()` for recursive nesting
- [x] Escapes special characters (\n, \r, \t, \\, \", control chars, DEL)
- [x] Thread-safe read-only inspection (const workspace pointer)

### 3.2: Implement ik_pp_cursor()
- [x] Created `src/workspace_cursor_pp.c`
- [x] Pretty-prints cursor structure recursively
- [x] Shows byte_offset and grapheme_offset
- [x] Uses generic helpers for consistent formatting

### 3.3: Tests
- [x] Created `tests/unit/workspace/pp_test.c` (344 lines, 8 tests)
- [x] Test coverage: empty workspace, single-line, multi-line, UTF-8, indentation, cursor positions, special characters
- [x] 100% coverage: 45/45 lines, 2/2 functions, 16/16 branches

## Task 4: REPL Integration ⏸️ DEFERRED to Phase 3

**Goal**: Add `/pp` command to REPL with visible output

**Why Deferred**:
Current implementation in `repl.c:162-165` outputs to stdout, which violates the fundamental design principle:

```c
// WRONG: Violates design principle
printf("%s", output);
fflush(stdout);
```

**Core Principle**: All visible output MUST go through screenbuffer → blit to alternate buffer. **NEVER stdout/stderr**.

**Blocker**: Without scrollback buffer (Phase 3), there is no proper way to display PP output adhering to this principle.

**Phase 3 Integration Plan**:
Once scrollback exists, `/pp` command will:
1. Format output using `ik_format_buffer_t` (already working ✅)
2. Append formatted output to scrollback buffer
3. Render scrollback + workspace in next frame
4. Output is visible and persistent

## Phase 2.75 Status Summary

- [x] Format module implemented with 100% test coverage
- [x] Generic PP helpers for reusable formatting
- [x] `ik_pp_workspace()` and `ik_pp_cursor()` with recursive nesting
- [x] 100% test coverage maintained (all tasks 1-3)
- [x] All quality gates pass: `make check && make lint && make coverage`
- [ ] `/pp` REPL integration - **DEFERRED to Phase 3** (requires scrollback buffer)
- [x] Ready to proceed to Phase 3

---

# Phase 3 - Scrollback Buffer Module

**Goal**: Add scrollback buffer storage with layout caching for historical output.

See [docs/repl/repl-phase-3.md](docs/repl/repl-phase-3.md) for detailed implementation plan.

## Task 1: Scrollback Module

**Create**: `src/scrollback.h` and `src/scrollback.c`

### 1.1: Design Data Structures (Red Step)
- [x] Write test first: `tests/unit/scrollback/scrollback_create_test.c`
  - Test: Create scrollback buffer with terminal width
  - Verify: Initial state (count=0, capacity>0, cached_width set)
- [x] Define types in `src/scrollback.h`:
  ```c
  typedef struct {
      size_t display_width;
      size_t physical_lines;
  } ik_line_layout_t;

  typedef struct ik_scrollback_t {
      char *text_buffer;
      size_t *text_offsets;
      size_t *text_lengths;
      ik_line_layout_t *layouts;
      size_t count;
      size_t capacity;
      int32_t cached_width;
      size_t total_physical_lines;
      size_t buffer_used;
      size_t buffer_capacity;
  } ik_scrollback_t;
  ```
- [x] Declare API:
  ```c
  res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                              ik_scrollback_t **scrollback_out);
  ```
- [x] Build and run test
- [x] **Red**: Test fails (function not implemented)

### 1.2: Implement Create (Green Step)
- [x] Implement `ik_scrollback_create()` in `src/scrollback.c`:
  - Allocate scrollback struct on parent context
  - Initialize all fields (count=0, capacity=16, etc.)
  - Allocate initial arrays (text_offsets, text_lengths, layouts)
  - Set cached_width from parameter
- [x] Build and run test
- [x] **Green**: Test passes

### 1.3: Implement Append Line (TDD)
- [x] Write test: `test_scrollback_append_single_line()`
  - Append "hello world"
  - Verify: count=1, text retrievable, display_width correct
- [x] Write test: `test_scrollback_append_multiple_lines()`
  - Append 3 lines
  - Verify: count=3, all text retrievable
- [x] Write test: `test_scrollback_append_utf8_content()`
  - Append lines with emoji, CJK, combining chars
  - Verify: display_width calculated correctly
- [x] Implement `ik_scrollback_append_line()`:
  - Grow arrays if needed (capacity doubling)
  - Copy text to text_buffer
  - Calculate display_width (scan UTF-8, call utf8proc_charwidth)
  - Calculate physical_lines (display_width / terminal_width)
  - Update total_physical_lines
- [x] All tests pass

### 1.4: Implement Layout Cache (TDD)
- [x] Write test: `test_scrollback_ensure_layout_no_change()`
  - Append lines, ensure_layout with same width
  - Verify: No recalculation (cached_width unchanged)
- [x] Write test: `test_scrollback_ensure_layout_resize()`
  - Append lines, ensure_layout with different width
  - Verify: physical_lines recalculated for all lines
- [x] Implement `ik_scrollback_ensure_layout()`:
  - If terminal_width == cached_width, return immediately
  - Recalculate physical_lines for all lines (O(n) arithmetic)
  - Update total_physical_lines
  - Update cached_width
- [x] All tests pass

### 1.5: Implement Query Functions (TDD)
- [x] Write tests for:
  - `ik_scrollback_get_line_count()`
  - `ik_scrollback_get_total_physical_lines()`
  - `ik_scrollback_get_line_text()`
  - `ik_scrollback_find_logical_line_at_physical_row()`
- [x] Implement all query functions
- [x] All tests pass

### 1.6: OOM Injection Tests
- [x] Test: `test_scrollback_create_oom()`
  - Inject malloc failure
  - Verify: Returns ERR with OOM code
- [x] Test: `test_scrollback_append_oom()`
  - Inject realloc failure during append
  - Verify: Returns ERR, scrollback state unchanged
- [x] All OOM tests pass

### 1.7: Verify Quality Gates
- [x] Run: `make check` (all scrollback tests pass)
- [x] Run: `make lint` (file size < 500 lines, complexity OK)
- [x] Run: `make coverage` (100% coverage)
- [x] Update Makefile: `LCOV_EXCL_COVERAGE` if needed

### 1.8: Create Commit
- [x] Commit: "Add scrollback module with layout caching (Phase 3 Task 1)"

---

## Task 2: Workspace Layout Caching ✅ COMPLETE

**Status**: Complete (2025-11-14)

### 2.1: Design Layout Cache (Red Step)
- [x] Write test: `tests/unit/workspace/layout_cache_test.c` (18 tests)
  - Test: `test_workspace_ensure_layout()`
  - Test: `test_workspace_invalidate_layout()`
  - Test: Initial state, clean cache, resize, empty/single/multi-line scenarios
  - Test: UTF-8 content, text modification invalidation
  - Test: Empty lines and zero-width characters
- [x] Update `src/workspace.h`:
  - Add fields: `physical_lines`, `cached_width`, `layout_dirty`
  - Declare: `ik_workspace_ensure_layout()`, `ik_workspace_invalidate_layout()`, `ik_workspace_get_physical_lines()`
- [x] Run test
- [x] **Red**: Test fails (as expected)

### 2.2: Implement Layout Cache (Green Step)
- [x] Create `src/workspace_layout.c` (130 lines)
- [x] Implement `ik_workspace_ensure_layout()`:
  - If !layout_dirty && cached_width == terminal_width, return (O(1) cache hit)
  - Scan workspace text (handle newlines, wrapping)
  - Calculate total physical_lines using UTF-8 display width
  - Update cached_width, clear layout_dirty
- [x] Implement `ik_workspace_invalidate_layout()`:
  - Set layout_dirty = 1
- [x] Update all text edit functions to call `invalidate_layout()`:
  - `workspace.c`: insert_codepoint, insert_newline, backspace, delete, clear, delete_word_backward
  - `workspace_multiline.c`: kill_to_line_end, kill_line
- [x] All tests pass (18/18)

### 2.3: Comprehensive Tests
- [x] Test: Layout calculation with various content (empty, single-line, multi-line, wrapping)
- [x] Test: Cache invalidation on insert/delete/backspace
- [x] Test: Terminal resize updates layout (80 → 40 cols)
- [x] Test: Multi-line text with newlines
- [x] Test: UTF-8 content (wide characters, emoji)
- [x] Test: Empty lines (just newlines) and zero-width characters
- [x] All tests pass

### 2.4: Verify Quality Gates
- [x] Run: `make check` (all 18 tests pass)
- [x] Run: `make lint` (workspace.c reduced to 493 lines, complexity OK)
- [x] Run: `make coverage` (100% lines, functions, branches)
- [x] Update Makefile: `LCOV_EXCL_COVERAGE = 229` (defensive UTF-8/NULL checks)

### 2.5: Create Commit
- [x] Commit: "Add workspace layout caching (Phase 3 Task 2)" (commit 53f3fde)

**Deliverables**:
- `src/workspace_layout.c` - 130 lines with UTF-8 aware layout calculation
- `tests/unit/workspace/layout_cache_test.c` - 18 comprehensive tests
- Updated workspace.h with layout cache fields
- 100% test coverage maintained

---

## Task 3: Performance Verification ✅ COMPLETE

**Status**: Complete (2025-11-14)

### 3.1: Create Performance Test
- [x] Create: `tests/performance/scrollback_reflow_perf.c` (303 lines, 4 tests)
  - Add 1000 lines to scrollback (50 chars avg)
  - Measure time for terminal resize (80→120 cols)
  - Target: < 5ms (ideally < 1ms)
- [x] Build and run performance test
- [x] Document results

**Performance Results**:
- **ASCII reflow (1000 lines, 80→120 cols)**: 0.003 ms ✅ PASS
  - **1,667× faster** than 5ms target
  - **Ideal target (< 1ms) achieved!**
- **UTF-8 reflow (1000 lines, 80→120 cols)**: 0.009 ms ✅ PASS
  - **556× faster** than 5ms target
  - Includes emoji and CJK characters
- **Multiple reflows** (5 different widths): All 0.003 ms ✅ PASS
- **Cache hit** (same width, no reflow): 0.000 ms ✅ PASS
  - O(1) width comparison as designed

**Test Implementation**:
- Created `tests/performance/scrollback_reflow_perf.c` with 4 test cases
- Uses `clock_gettime(CLOCK_MONOTONIC)` for high-precision timing
- Tests ASCII content, UTF-8 content, multiple reflows, and cache hits
- Updated Makefile with `check-performance` target
- All 4 tests pass with excellent performance

### 3.2: Optional Profiling
- [x] Performance target exceeded - profiling not needed
  - Reflow performance is 556-1667× faster than target
  - O(1) arithmetic reflow design validated

---

## Task 4: Manual Verification ✅ COMPLETE

**Status**: Complete (2025-11-14)

### 4.1: Update client.c Demo
- [x] Create demo in `src/client.c` main():
  - Create scrollback buffer
  - Add lines with various content (ASCII, UTF-8, long lines)
  - Query total physical lines
  - Simulate resize, print timing
  - Print some lines back
- [x] Build: `make all`
- [x] Run: `./bin/ikigai`

### 4.2: Verification Checklist
- [x] Lines stored correctly (10 logical lines, retrieved successfully)
- [x] Display width calculated correctly for UTF-8 (emoji, CJK, combining chars)
- [x] Physical line counts correct (11 physical lines at 80 cols, varies with width)
- [x] Resize updates physical_lines (120→11, 40→14, 60→12 cols)
- [x] Performance excellent (0.006880 ms for 1000+ lines, **726× better than 5ms target**)
- [x] No memory leaks (valgrind: 0 bytes at exit, 31 allocs = 31 frees, 0 errors)

**Verification Results**:
- Scrollback stores and retrieves lines correctly
- UTF-8 display width calculation accurate
- Terminal resize updates layouts correctly (O(1) arithmetic reflow)
- Cache hit works as expected (~0.000 ms)
- Performance far exceeds target (< 0.01 ms vs 5ms target)
- Memory management perfect (no leaks, no errors)

---

## Phase 3 Completion Checklist

- [x] Scrollback module implemented (100% coverage)
- [x] Workspace layout caching implemented (100% coverage)
- [x] All unit tests pass
- [x] Performance target met (1000 lines < 5ms) - **0.003-0.009 ms achieved!**
- [x] Manual verification complete (0 memory leaks, all tests pass)
- [x] Quality gates pass: `make check && make lint && make coverage`
- [x] **Phase 3 COMPLETE - Ready for Phase 4**

---

# Phase 4 - Viewport and Scrolling Integration

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

See [docs/repl/repl-phase-4.md](docs/repl/repl-phase-4.md) for detailed implementation plan.

## Summary

- Integrate scrollback buffer into REPL context
- Implement `ik_repl_submit_line()` - move workspace to scrollback
- Implement `ik_repl_scroll()` - adjust viewport
- Update `ik_render_frame()` - render scrollback + workspace
- Add Page Up/Down input actions
- 100% test coverage requirement

---

# Phase 5 - Cleanup and Documentation

**Goal**: Remove vterm from build system, update all documentation, finalize implementation.

See plan.md for detailed checklist.

## Summary

- Remove vterm from distro packaging files
- Update architecture documentation
- Run distro verification builds
- Manual testing on multiple terminal emulators
- Final polish and completion
