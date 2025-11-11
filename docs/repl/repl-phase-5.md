# REPL Terminal - Phase 5: Cleanup and Polish

[← Back to REPL Terminal Overview](README.md)

**Goal**: Final polish, code review, documentation updates, and multi-distro verification.

**Status**: Not started (after Phase 4)

## Rationale

Phases 0-4 deliver full REPL functionality. Phase 5 ensures production quality before optional enhancements (Phase 6) or moving to next major features (LLM integration, database).

This phase is not about new features—it's about quality, consistency, and maintainability.

## Implementation Tasks

### Task 5.1: Code Review and Refactoring

**Goal**: Review all REPL code for quality, consistency, and maintainability.

**Review checklist**:
- [ ] Consistent error handling patterns
- [ ] Consistent naming conventions (see `docs/naming.md`)
- [ ] Proper talloc memory hierarchy (see `docs/memory.md`)
- [ ] K&R style formatting (120-char width)
- [ ] No dead code or commented-out blocks
- [ ] All TODOs resolved or documented
- [ ] No magic numbers (use named constants)
- [ ] Consistent function ordering (public API first, internal helpers after)

**Refactoring targets**:
- Extract common patterns into utilities
- Simplify complex functions (aim for < 50 lines)
- Improve variable/function names for clarity
- Add missing assertions for invariants
- Consolidate duplicate logic

**Files to review**:
- `src/repl.{c,h}`
- `src/workspace.{c,h}`
- `src/workspace_cursor.{c,h}`
- `src/workspace_multiline.c`
- `src/render_direct.{c,h}`
- `src/scrollback.{c,h}` (from Phase 3)
- `src/input.{c,h}`
- `src/terminal.{c,h}`

---

### Task 5.2: Documentation Updates

**Goal**: Ensure all documentation is accurate, complete, and up-to-date.

**Update**:
- [ ] `docs/repl/README.md` - Mark phases 0-5 complete, update overview if needed
- [ ] `docs/architecture.md` - Update REPL section with final architecture
- [ ] `docs/README.md` - Update project status and roadmap
- [ ] All phase docs (0-5) - Add "COMPLETE" status markers
- [ ] Code comments - Ensure all public APIs have clear docstrings
- [ ] Function headers - Consistent `@brief`, `@param`, `@return` format

**Add if missing**:
- Architecture diagrams (ASCII art of data flow)
- Performance characteristics documentation
- Known limitations section
- Future enhancement ideas (feeding into Phase 6 or beyond)

---

### Task 5.3: Test Coverage Verification

**Goal**: Verify 100% test coverage for all REPL modules.

**Coverage report**:
```bash
make coverage
# Review coverage/index.html
# Target: 100% line coverage for all REPL modules
```

**Ensure coverage for**:
- All public API functions
- All error paths
- All edge cases (empty input, boundary conditions, UTF-8 edge cases)
- OOM injection tests via test seams
- Integration tests for complete workflows

**If coverage < 100%**:
- Add missing tests
- Remove unreachable code
- Mark genuinely untestable code with `LCOV_EXCL_LINE`

---

### Task 5.4: Valgrind Clean

**Goal**: Zero Valgrind errors/warnings for all REPL code.

**Run Valgrind on all tests**:
```bash
make valgrind
# Review output for any issues
```

**Common issues to check**:
- Memory leaks (should be zero with talloc hierarchy)
- Uninitialized memory reads
- Invalid memory access
- File descriptor leaks (terminal, /dev/tty)

**Fix any issues found**:
- Ensure proper talloc cleanup
- Check for missing error handling
- Verify terminal cleanup in all exit paths

---

### Task 5.5: Multi-Distro Verification

**Goal**: Verify REPL works correctly across all target distributions.

**Test on**:
- Debian 13 (Trixie) - primary target
- Ubuntu 24.04 LTS (if available)
- Fedora latest (different terminal defaults)

**Verify**:
- Clean build with `make clean && make`
- All tests pass: `make test`
- Valgrind clean: `make valgrind`
- Coverage 100%: `make coverage`
- Manual testing works correctly

**Manual test checklist**:
```bash
# Basic functionality
1. Launch ikigai REPL
2. Type single-line text → verify display
3. Type multi-line text → verify wrapping
4. Arrow keys left/right → verify cursor movement
5. Arrow keys up/down → verify line navigation
6. Ctrl+A → verify jump to line start
7. Ctrl+E → verify jump to line end
8. Ctrl+K → verify kill to end of line
9. Ctrl+U → verify kill entire line
10. Ctrl+W → verify delete word backward
11. Backspace/Delete → verify character deletion
12. Terminal resize → verify layout updates
13. Page Up/Down → verify scrollback scrolling
14. Ctrl+C → verify clean exit

# Edge cases
15. Type emoji (🚀) → verify proper display width
16. Type CJK characters (中文) → verify proper display width
17. Paste multi-line text → verify formatting
18. Resize to very small (80x10) → verify no crashes
19. Resize to very large (200x60) → verify proper rendering
20. Fill scrollback with 100+ lines → verify scrolling works
```

---

### Task 5.6: Performance Verification

**Goal**: Verify performance characteristics meet design targets.

**Measure**:
```bash
# Terminal resize performance (Phase 3 target: 1000× faster)
# - Create scrollback with 1000 lines
# - Measure reflow time on resize
# - Target: < 10μs (vs 2.5ms naive approach)

# Frame render time
# - Measure time from input action to terminal write
# - Target: < 16ms (60fps) for typical workspace size
# - Target: < 100ms worst case with full scrollback

# Memory usage
# - Measure REPL context size with 1000 scrollback lines
# - Target: < 1MB for typical usage
```

**Tools**:
- Use `clock_gettime()` for microbenchmarks
- Use Valgrind Massif for memory profiling
- Add performance test suite if needed

---

### Task 5.7: Build System Verification

**Goal**: Ensure build system works correctly for REPL modules.

**Verify**:
- [ ] All REPL modules included in `SOURCES` (Makefile)
- [ ] All REPL tests included in `TEST_SOURCES` (Makefile)
- [ ] Dependencies tracked correctly (`.d` files)
- [ ] Incremental builds work (only rebuild changed files)
- [ ] Clean builds work (`make clean && make`)
- [ ] All quality gates pass:
  - `make` - build binary
  - `make test` - run tests
  - `make valgrind` - memory checks
  - `make coverage` - coverage report
  - `make sanitize` - sanitizer build

---

### Task 5.8: Final Integration Test

**Goal**: End-to-end test of complete REPL workflow.

**Test scenario**:
```bash
# Simulate real usage session
1. Launch REPL
2. Type multi-line text (3-4 lines)
3. Edit with cursor movement and deletion
4. Resize terminal
5. Submit input (adds to scrollback)
6. Repeat 10 times (build scrollback)
7. Scroll through history
8. Type new input
9. Clean exit

# Verify:
- No memory leaks
- No visual glitches
- Smooth performance
- Correct terminal cleanup on exit
```

---

## Success Criteria

- ✅ All code reviewed and refactored for quality
- ✅ All documentation updated and accurate
- ✅ 100% test coverage maintained
- ✅ Zero Valgrind errors/warnings
- ✅ Works on all target distros (Debian 13+)
- ✅ Performance targets met
- ✅ Build system clean and correct
- ✅ End-to-end integration test passes

---

## Deliverables

**Code**:
- Clean, consistent, well-documented REPL implementation
- Full test suite with 100% coverage
- No memory leaks, no warnings

**Documentation**:
- Updated phase docs (0-5 marked complete)
- Updated architecture documentation
- Updated project README

**Quality**:
- Passes all quality gates
- Works on all target platforms
- Ready for production use or Phase 6 enhancements

---

## Notes

**Phase 5 is the "Definition of Done" for core REPL**.

After Phase 5:
- Core REPL is production-ready
- Can proceed to Phase 6 (optional enhancements)
- Can proceed to LLM integration
- Can proceed to database integration
- Foundation is solid for all future work

**No new features in Phase 5** - only quality and polish. If new features are needed, they go in Phase 6 or later.
