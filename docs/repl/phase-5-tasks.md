# REPL Implementation Tasks

This file tracks task details for REPL phases. For high-level overview see [plan.md](plan.md).

---

## Completed Phases (Summary)

### Phase 2.5 - Remove Server and Protocol Code ✅
**Complete**: 2025-11-13 - Removed ~1,483 lines of server/protocol code.

### Phase 2.75 - Pretty-Print Infrastructure ✅
**Complete**: 2025-11-13 - Format module and PP functions ready (~1000 lines). `/pp` command integration deferred to Phase 5.

### Phase 3 - Scrollback Buffer Module ✅
**Complete**: 2025-11-14 - O(1) arithmetic reflow, 726× better than target. 100% coverage (1,569 lines, 126 functions, 554 branches).

### Phase 4 - Viewport and Scrolling Integration ✅
**Complete**: 2025-11-14 - Automated testing done (11 tests, 100% coverage: 1,648 lines, 128 functions, 510 branches).

---

## Phase 5 - Cleanup and Documentation

**Status**: ⏳ IN PROGRESS

**Remaining Tasks**:

1. **Manual Testing** ✅
   - [x] Test Phase 4 scrolling (Page Up/Down, auto-scroll on submit)
   - [x] Test `/pp` command integration (requires scrollback output)
   - [x] Test on multiple terminal emulators (xterm, gnome-terminal, alacritty, kitty)

2. **Build System Cleanup**
   - [ ] Update distro packaging files (Debian, Fedora, Arch)
   - [ ] Run `make distro-check` to verify builds

3. **Documentation**
   - [ ] Update `docs/architecture.md` (final dependencies list)
   - [ ] Update `docs/repl/README.md` (final status)
   - [ ] Archive plan.md and tasks.md (move to docs/repl/)

4. **Quality Gates**
   - [ ] Final run: `make check && make lint && make coverage && make check-dynamic`
