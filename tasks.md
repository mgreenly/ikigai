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
- [ ] Delete: `src/protocol.c` (294 lines)
- [ ] Delete: `src/protocol.h` (38 lines)
- [ ] Remove from Makefile: `MODULE_SOURCES` reference to `src/protocol.c`

### 1.2: Delete Protocol Tests
- [ ] Delete: `tests/unit/protocol/` directory (4 test files)
  - `parse_test.c`
  - `serialize_test.c`
  - `create_test.c`
  - `uuid_test.c`
- [ ] Delete: `tests/integration/protocol_integration_test.c`
- [ ] Remove from Makefile: All protocol test targets and build rules

### 1.3: Quality Check
- [ ] Build: `make clean && make build/ikigai`
- [ ] Verify: Client builds successfully without protocol module
- [ ] Run: `make check` (all remaining tests pass)

---

## Task 2: Remove Server Binary

### 2.1: Delete Server Source
- [ ] Delete: `src/server.c` (41 lines)
- [ ] Remove from Makefile: `SERVER_SOURCES`, `SERVER_TARGET`, server build rules
- [ ] Remove from Makefile: Install/uninstall rules for ikigai-server

### 2.2: Update Build System
- [ ] Update Makefile `all` target: Remove ikigai-server
- [ ] Update Makefile `clean` target: Remove server artifacts
- [ ] Update Makefile help text: Remove server references
- [ ] Verify: `make all` builds only client binary

### 2.3: Quality Check
- [ ] Build: `make clean && make all`
- [ ] Verify: Only `bin/ikigai` is built (no ikigai-server)
- [ ] Run: `make check && make lint`

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
- [ ] Add comment to `src/config.c`: "Jansson used for config parsing only. Will revisit JSON library during Phase 3 (OpenAI integration)."
- [ ] Keep `src/wrapper.{c,h}` test seam for `json_is_object()`
- [ ] Keep jansson in Makefile dependencies
- [ ] Update `docs/architecture.md`: Document jansson scope (config only)

### 3.2: Verify Config Still Works
- [ ] Build: `make clean && make build/ikigai`
- [ ] Run: Config tests pass
- [ ] Manual: Verify config loading works with test file

---

## Task 4: Update Documentation

### 4.1: Update Architecture Docs
- [ ] Update `docs/architecture.md`:
  - Remove server/protocol references from component overview
  - Remove WebSocket/protocol message flow diagrams
  - Clarify desktop client-only architecture
  - Document jansson scope (config only, temporary)
- [ ] Update `docs/README.md`:
  - Remove server/protocol from roadmap
  - Clarify v1.0 is desktop client only

### 4.2: Update Build Documentation
- [ ] Update `docs/build-system.md`:
  - Remove server build targets
  - Remove protocol test references
  - Update dependency list (jansson scope)

### 4.3: Archive Old Design Docs
- [ ] Create `docs/archive/` directory if it doesn't exist
- [ ] Move to `docs/archive/`:
  - `docs/decisions/websocket-communication.md` (server architecture decision)
  - `docs/decisions/close-websocket-on-error.md` (server error handling)
- [ ] Review other docs/decisions/*.md files for server/protocol references
- [ ] Update `docs/decisions/README.md` if it lists archived decisions

---

## Task 5: Verification and Commit

### 5.1: Final Quality Gates
- [ ] Run: `make clean`
- [ ] Run: `make all` (builds only ikigai client)
- [ ] Run: `make check` (all tests pass)
- [ ] Run: `make lint` (complexity checks pass)
- [ ] Run: `make coverage` (100% coverage maintained)
- [ ] Run: `make check-dynamic` (ASan, UBSan, TSan pass)
- [ ] Manual: `./bin/ikigai` launches and works

### 5.2: Code Statistics
- [ ] Count lines removed:
  - `src/protocol.{c,h}` (~332 lines)
  - `src/server.c` (~41 lines)
  - Protocol tests (~400+ lines estimated)
  - Total: ~773+ lines removed
- [ ] Verify Makefile cleanup (server targets removed)
- [ ] Verify documentation updated

### 5.3: Create Commit
- [ ] Git add deletions and documentation updates
- [ ] Commit message:
  ```
  Remove server and protocol code (Phase 2.5)

  v1.0 is a desktop client that connects directly to LLM APIs.
  The server/protocol code was from an earlier architecture and
  is no longer needed. This removes ~773 lines of code and
  eliminates maintenance burden before Phase 3.

  Removed:
  - src/protocol.{c,h} (332 lines)
  - src/server.c (41 lines)
  - tests/unit/protocol/ (4 test files)
  - tests/integration/protocol_integration_test.c
  - Makefile server targets and build rules

  Kept:
  - jansson in config.c (will revisit in Phase 3)
  - wrapper.c test seam for json_is_object()

  Updated:
  - docs/architecture.md (desktop client only)
  - docs/README.md (removed server references)
  - docs/build-system.md (removed server targets)
  ```

### 5.4: Verify Post-Commit
- [ ] Run: `make clean && make all`
- [ ] Run: `make check && make lint && make coverage`
- [ ] Verify: All quality gates pass
- [ ] Ready for Phase 3

---

## Phase 2.5 Completion Checklist

- [x] Protocol module removed (src + tests)
- [x] Server binary removed (src + Makefile targets)
- [x] Jansson scope documented (config only)
- [x] Documentation condensed (jansson proposal, memory analysis)
- [x] All quality gates pass (check, lint, coverage)
- [x] Commit created: 3169dc3
- [x] 1,483 net lines removed (1,775 deletions - 292 insertions)
- [x] Ready to proceed to Phase 3

---

# Phase 3 - Scrollback Buffer Module

**Goal**: Add scrollback buffer storage with layout caching for historical output.

See [docs/repl/repl-phase-3.md](docs/repl/repl-phase-3.md) for detailed implementation plan.

## Task 1: Scrollback Module

**Create**: `src/scrollback.h` and `src/scrollback.c`

### 1.1: Design Data Structures (Red Step)
- [ ] Write test first: `tests/unit/scrollback/scrollback_create_test.c`
  - Test: Create scrollback buffer with terminal width
  - Verify: Initial state (count=0, capacity>0, cached_width set)
- [ ] Define types in `src/scrollback.h`:
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
- [ ] Declare API:
  ```c
  res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                              ik_scrollback_t **scrollback_out);
  ```
- [ ] Build and run test
- [ ] **Red**: Test fails (function not implemented)

### 1.2: Implement Create (Green Step)
- [ ] Implement `ik_scrollback_create()` in `src/scrollback.c`:
  - Allocate scrollback struct on parent context
  - Initialize all fields (count=0, capacity=16, etc.)
  - Allocate initial arrays (text_offsets, text_lengths, layouts)
  - Set cached_width from parameter
- [ ] Build and run test
- [ ] **Green**: Test passes

### 1.3: Implement Append Line (TDD)
- [ ] Write test: `test_scrollback_append_single_line()`
  - Append "hello world"
  - Verify: count=1, text retrievable, display_width correct
- [ ] Write test: `test_scrollback_append_multiple_lines()`
  - Append 3 lines
  - Verify: count=3, all text retrievable
- [ ] Write test: `test_scrollback_append_utf8_content()`
  - Append lines with emoji, CJK, combining chars
  - Verify: display_width calculated correctly
- [ ] Implement `ik_scrollback_append_line()`:
  - Grow arrays if needed (capacity doubling)
  - Copy text to text_buffer
  - Calculate display_width (scan UTF-8, call utf8proc_charwidth)
  - Calculate physical_lines (display_width / terminal_width)
  - Update total_physical_lines
- [ ] All tests pass

### 1.4: Implement Layout Cache (TDD)
- [ ] Write test: `test_scrollback_ensure_layout_no_change()`
  - Append lines, ensure_layout with same width
  - Verify: No recalculation (cached_width unchanged)
- [ ] Write test: `test_scrollback_ensure_layout_resize()`
  - Append lines, ensure_layout with different width
  - Verify: physical_lines recalculated for all lines
- [ ] Implement `ik_scrollback_ensure_layout()`:
  - If terminal_width == cached_width, return immediately
  - Recalculate physical_lines for all lines (O(n) arithmetic)
  - Update total_physical_lines
  - Update cached_width
- [ ] All tests pass

### 1.5: Implement Query Functions (TDD)
- [ ] Write tests for:
  - `ik_scrollback_get_line_count()`
  - `ik_scrollback_get_total_physical_lines()`
  - `ik_scrollback_get_line_text()`
  - `ik_scrollback_find_logical_line_at_physical_row()`
- [ ] Implement all query functions
- [ ] All tests pass

### 1.6: OOM Injection Tests
- [ ] Test: `test_scrollback_create_oom()`
  - Inject malloc failure
  - Verify: Returns ERR with OOM code
- [ ] Test: `test_scrollback_append_oom()`
  - Inject realloc failure during append
  - Verify: Returns ERR, scrollback state unchanged
- [ ] All OOM tests pass

### 1.7: Verify Quality Gates
- [ ] Run: `make check` (all scrollback tests pass)
- [ ] Run: `make lint` (file size < 500 lines, complexity OK)
- [ ] Run: `make coverage` (100% coverage)
- [ ] Update Makefile: `LCOV_EXCL_COVERAGE` if needed

### 1.8: Create Commit
- [ ] Commit: "Add scrollback module with layout caching (Phase 3 Task 1)"

---

## Task 2: Workspace Layout Caching

### 2.1: Design Layout Cache (Red Step)
- [ ] Write test: `tests/unit/workspace/layout_cache_test.c`
  - Test: `test_workspace_ensure_layout()`
  - Test: `test_workspace_invalidate_layout()`
- [ ] Update `src/workspace.h`:
  - Add fields: `physical_lines`, `cached_width`, `layout_dirty`
  - Declare: `ik_workspace_ensure_layout()`, `ik_workspace_invalidate_layout()`, `ik_workspace_get_physical_lines()`
- [ ] Run test
- [ ] **Red**: Test fails

### 2.2: Implement Layout Cache (Green Step)
- [ ] Implement `ik_workspace_ensure_layout()`:
  - If !layout_dirty && cached_width == terminal_width, return
  - Scan workspace text (handle newlines, wrapping)
  - Calculate total physical_lines
  - Update cached_width, clear layout_dirty
- [ ] Implement `ik_workspace_invalidate_layout()`:
  - Set layout_dirty = true
- [ ] Update all text edit functions to call `invalidate_layout()`
- [ ] All tests pass

### 2.3: Comprehensive Tests
- [ ] Test: Layout calculation with various content
- [ ] Test: Cache invalidation on insert/delete/backspace
- [ ] Test: Terminal resize updates layout
- [ ] Test: Multi-line text with newlines
- [ ] All tests pass

### 2.4: Verify Quality Gates
- [ ] Run: `make check`
- [ ] Run: `make lint`
- [ ] Run: `make coverage` (100%)

### 2.5: Create Commit
- [ ] Commit: "Add workspace layout caching (Phase 3 Task 2)"

---

## Task 3: Performance Verification

### 3.1: Create Performance Test
- [ ] Create: `tests/performance/scrollback_reflow_perf.c`
  - Add 1000 lines to scrollback (50 chars avg)
  - Measure time for terminal resize (80→120 cols)
  - Target: < 5ms (ideally < 1ms)
- [ ] Build and run performance test
- [ ] Document results

### 3.2: Optional Profiling
- [ ] If performance target not met, profile with perf/gprof
- [ ] Identify bottlenecks
- [ ] Optimize if needed

---

## Task 4: Manual Verification

### 4.1: Update client.c Demo
- [ ] Create demo in `src/client.c` main():
  - Create scrollback buffer
  - Add lines with various content (ASCII, UTF-8, long lines)
  - Query total physical lines
  - Simulate resize, print timing
  - Print some lines back
- [ ] Build: `make all`
- [ ] Run: `./bin/ikigai`

### 4.2: Verification Checklist
- [ ] Lines stored correctly
- [ ] Display width calculated correctly for UTF-8
- [ ] Physical line counts correct
- [ ] Resize updates physical_lines
- [ ] Performance acceptable (1000 lines < 5ms)
- [ ] No memory leaks (valgrind)

---

## Phase 3 Completion Checklist

- [ ] Scrollback module implemented (100% coverage)
- [ ] Workspace layout caching implemented (100% coverage)
- [ ] All unit tests pass
- [ ] Performance target met (1000 lines < 5ms)
- [ ] Manual verification complete
- [ ] Quality gates pass: `make check && make lint && make coverage`
- [ ] Ready for Phase 4

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
