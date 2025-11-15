# REPL Terminal - Testing Strategy

[← Back to REPL Terminal Overview](README.md)

**Development approach**: Strict TDD (Test-Driven Development)

All code is developed using TDD red/green cycle:
1. **Red**: Write failing test first
2. **Green**: Write minimal code to pass test
3. **Verify**: Run `make check`, `make lint`, `make coverage`
4. **Coverage requirement**: 100% line, function, and branch coverage

**Manual testing**: Performed AFTER TDD is complete for each phase to validate full integration.

## Manual Test Plan (Phase 1 - Direct Rendering)

Once Phase 1 is complete and all TDD tests pass with 100% coverage, validate via client.c demo:

### Direct Rendering Demo
- Launch demo with render context
- Type some text, verify it displays correctly
- Verify cursor appears at correct position
- Test text wrapping at terminal boundary
- Type UTF-8 characters (emoji 🎉, CJK 你好, combining é)
- Verify terminal restores cleanly on exit (Ctrl+C)

---

## Manual Test Plan (Phase 2 - REPL Event Loop)

Once Phase 2 is complete and all TDD tests pass, validate the complete system manually:

### 1. Launch and basic operation
- Launch `./ikigai`
- Verify alternate screen activated
- Type some text, verify it appears
- Exit with Ctrl+C, verify terminal restored cleanly

### 2. UTF-8 handling
- Type emoji: 🎉 👨‍👩‍👧‍👦
- Verify they display correctly
- Type combining characters: e + ´ = é
- Use left/right arrows through multi-byte characters
- Verify cursor moves by whole grapheme clusters

### 3. Text editing
- Type "hello world"
- Move cursor to middle with arrow keys
- Insert characters in middle
- Backspace and delete characters
- Verify editing works correctly

### 4. Multi-line input
- Type text
- Press Enter to insert newline
- Continue typing on next line
- Verify text wraps correctly
- Use arrow keys to move around (left/right/up/down)

### 5. Edge cases
- Fill entire screen with text
- Verify wrapping continues to work
- Try very long lines
- Try rapid typing
- Try holding down arrow keys

---

## Manual Test Plan (Phase 3 - Scrollback Buffer)

Once Phase 3 is complete and all TDD tests pass, validate via client.c demo:

### Scrollback Buffer Demo
- Create scrollback buffer with 1000+ lines
- Add lines with various UTF-8 content (ASCII, CJK, emoji)
- Add long lines that require wrapping
- Query total physical lines
- Simulate terminal resize (change terminal_width parameter)
- Verify reflow recalculates correctly
- Measure reflow performance (should be < 5ms for 1000 lines)
- Verify no memory leaks with talloc

---

## Manual Test Plan (Phase 4 - Viewport and Scrolling)

Once Phase 4 is complete and all TDD tests pass, validate the complete REPL with scrollback:

### 1. Basic scrollback operation
- Launch `./ikigai`
- Enter several lines (press Enter to submit to scrollback)
- Verify lines appear in scrollback area
- Verify separator line between scrollback and input buffer
- Exit, verify terminal restored cleanly

### 2. Scrolling behavior
- Fill scrollback with many lines
- Use Page Up to scroll up through history
- Verify input buffer + separator disappear as you scroll
- While scrolled up, type a character
- Verify viewport snaps back to show cursor at bottom
- Test scroll bounds: can't scroll above first line or past bottom

### 3. Terminal resize
- Fill scrollback with content
- Resize terminal window
- Verify content reflows correctly
- Verify no corruption or display issues
