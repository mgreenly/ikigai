# Prompt for Next Session

Copy and paste this to start the next session:

---

We completed manual testing of the Phase 4 REPL implementation and found 3 bugs documented in BUGS.md:

**Bug #1** (Minor): Initial render - no separator line, cursor at wrong position
- Expected: Separator visible on startup, cursor below it at row 1
- Current: Blank screen, cursor at 0,0

**Bug #2** (Minor): Slash commands echo to scrollback
- Expected: `/pp` shows only workspace debug output
- Current: Shows both debug output AND "/pp" text

**Bug #3** (CRITICAL): Crash after Ctrl+U
- Reproduction: Type text, press Ctrl+U, press any key → crash
- Error: `Assertion 'index <= array->size' failed` in src/array.c:94
- Affected: `ik_workspace_kill_line()` in src/workspace_multiline.c:366
- Note: Ctrl+K works fine, only Ctrl+U crashes

Please fix these bugs in priority order (Bug #3 first). Follow TDD:
1. Write failing test that reproduces the bug
2. Fix the bug
3. Verify test passes
4. Run `make check && make lint && make coverage`
5. Manual test to confirm fix

Start with Bug #3 (the crash).
