# REPL Terminal - Testing Strategy

[← Back to REPL Terminal Overview](repl-terminal.md)

**Development approach**: Strict TDD (Test-Driven Development)

All code is developed using TDD red/green cycle:
1. **Red**: Write failing test first
2. **Green**: Write minimal code to pass test
3. **Verify**: Run `make check`, `make lint`, `make coverage`
4. **Coverage requirement**: 100% line, function, and branch coverage

**Manual testing**: Performed AFTER TDD is complete for each feature to validate full integration.

## Manual Test Plan (Phase 2 - Full UI)

Once Phase 2 is complete and all TDD tests pass, validate the complete system manually:

### 1. Launch and basic operation
- Launch app, verify alternate screen
- Type lines, press Enter, verify they move to scrollback
- Exit app, verify terminal restored cleanly

### 2. UTF-8 and grapheme handling
- Type emoji (🎉, 👨‍👩‍👧‍👦), verify they display correctly
- Type combining characters (e + ´ = é)
- Use left/right arrows to move through multi-byte characters
- Verify cursor moves by whole grapheme clusters (not bytes)
- Insert text in middle of emoji sequence, verify no corruption
- Backspace over multi-byte characters, verify they delete as units

### 3. Multi-line dynamic zone
- Type multi-line text, verify wrapping works correctly
- Use arrow keys to move cursor around in multi-line text
- Verify up/down arrows move between wrapped lines

### 4. Scrolling behavior
- Fill screen with lines, use mouse wheel to scroll up through history
- Verify dynamic zone + separator disappear line-by-line as you scroll up
- While scrolled up (dynamic zone off-screen), type a character
- Verify viewport snaps back to show cursor at bottom
- Test scroll bounds: can't scroll above first line or past bottom

### 5. Advanced features
- Test Page Up/Down scrolling
- Type very long text in dynamic zone (taller than screen)
- Verify internal scrolling within dynamic zone works
