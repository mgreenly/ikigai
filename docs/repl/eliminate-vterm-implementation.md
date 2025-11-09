# Eliminate libvterm: Implementation Plan

This document describes the implementation plan and testing strategy.

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

