# Phase 0 Tasks: Foundation ✅ COMPLETE

All Phase 0 tasks completed with 100% test coverage.

---

# Phase 1 Tasks: Simple Dynamic Zone REPL

See [docs/repl-terminal.md](docs/repl-terminal.md) for complete design.

**Goal**: Build minimal interactive terminal with just a workspace (no scrollback buffer). Validate terminal fundamentals: raw mode, UTF-8, cursor handling, vterm rendering.

**Features**:
- Terminal setup (raw mode, alternate screen)
- Single workspace using `ik_byte_array_t`
- Text input (typing characters)
- UTF-8 and grapheme cluster handling via libutf8proc
- Cursor tracking (dual offset: byte + grapheme)
- Cursor movement (arrow keys)
- Text editing (insert at cursor, backspace/delete)
- Enter inserts newline (no submission yet)
- Basic rendering with vterm (compose + blit)
- Exit on Ctrl+C

**What we defer to Phase 2**:
- Scrollback buffer
- Viewport scrolling
- Separator line
- Mouse input

---

## Task 1: Terminal Module (Setup and Cleanup)

Create terminal module for raw mode and alternate screen management.

**Files**: `src/terminal.h`, `src/terminal.c`, `tests/unit/terminal_test.c`

### Step 1: Terminal Context Structure ✅

- [x] Create `src/terminal.h` header
- [x] Define `ik_term_ctx_t` structure:
  - `int tty_fd` - Terminal file descriptor
  - `struct termios orig_termios` - Original terminal settings
  - `int screen_rows` - Terminal height
  - `int screen_cols` - Terminal width
- [x] Add function declarations:
  - `res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)`
  - `void ik_term_cleanup(ik_term_ctx_t *ctx)`
  - `res_t ik_term_get_size(ik_term_ctx_t *ctx, int *rows_out, int *cols_out)`

### Step 2: Terminal Initialization (Raw Mode + Alternate Screen) ✅ COMPLETE

**Status**: Implementation complete with 100% coverage via MOCKABLE system call wrappers.

- [x] Create `src/terminal.c` implementation
- [x] Implement `ik_term_init()`:
  - Open `/dev/tty` (fail if not available)
  - Get original termios settings (`tcgetattr`)
  - Set raw mode (disable canonical, echo, signals)
  - Enter alternate screen buffer (`\x1b[?1049h`)
  - Get terminal size (`ioctl TIOCGWINSZ`)
  - Allocate context with talloc
- [x] Write test `test_term_init_raw_mode()` in `tests/unit/terminal_test.c`:
  - Verify context created successfully
  - Verify tty_fd is valid
  - Verify screen dimensions are reasonable (> 0)
- [x] Write test `test_term_init_oom()`:
  - Test OOM scenarios using `oom_test_*` helpers
- [x] Implement `ik_term_cleanup()`:
  - Exit alternate screen buffer (`\x1b[?1049l`)
  - Restore original termios settings (`tcsetattr`)
  - Close tty file descriptor
- [x] Write test `test_term_cleanup_null_safe()`:
  - Verify cleanup handles NULL gracefully
- [x] Implement `ik_term_get_size()`:
  - Use `ioctl TIOCGWINSZ` to get current dimensions
  - Update ctx->screen_rows and ctx->screen_cols
  - Return dimensions via out parameters
- [x] Write test `test_term_get_size()`:
  - Initialize terminal
  - Query size
  - Verify rows and cols are reasonable
  - Verify values match context fields
- [x] Write assertion tests for NULL parameters
- [x] Update Makefile to include terminal.c in build
- [x] `make check` passes
- [x] `make lint` passes
- [x] `make coverage` passes with 100% (lines, functions, branches)

**Solution**: Created MOCKABLE wrappers for system calls (open, close, tcgetattr, tcsetattr, ioctl, write) following existing wrapper pattern. Assertions excluded from branch coverage using LCOV_EXCL_BR_LINE (tested via SIGABRT tests).

### Step 3: Terminal Cleanup (Restore State) ✅ COMPLETE

Merged into Step 2 above.

### Step 4: Terminal Size Query ✅ COMPLETE

Merged into Step 2 above.

### Step 5: Demo in client.c ✅ COMPLETE

- [x] Update `src/client.c` to demonstrate terminal module:
  - Initialize terminal (raw mode + alternate screen)
  - Display terminal dimensions on screen
  - Display "Press Ctrl+C to exit" message
  - Read bytes in loop until Ctrl+C (0x03)
  - Cleanup terminal and exit
- [x] Build and manually test:
  - `make && ./ikigai`
  - Verify alternate screen activates
  - Verify dimensions display
  - Press Ctrl+C, verify clean exit and terminal restore
- [x] Commit work: "Demo terminal module in client.c"

---

## Task 2: Input Parser Module

Create input parser to convert raw byte sequences into semantic actions.

**Files**: `src/input.h`, `src/input.c`, `tests/unit/input_test.c`

### Step 1: Input Action Types ✅

- [x] Create `src/input.h` header
- [x] Define `ik_input_action_type_t` enum:
  - `IK_INPUT_CHAR` - Regular character
  - `IK_INPUT_NEWLINE` - Enter key
  - `IK_INPUT_BACKSPACE` - Backspace key
  - `IK_INPUT_DELETE` - Delete key
  - `IK_INPUT_ARROW_LEFT` - Left arrow
  - `IK_INPUT_ARROW_RIGHT` - Right arrow
  - `IK_INPUT_ARROW_UP` - Up arrow
  - `IK_INPUT_ARROW_DOWN` - Down arrow
  - `IK_INPUT_CTRL_C` - Ctrl+C (exit)
  - `IK_INPUT_UNKNOWN` - Unrecognized sequence
- [x] Define `ik_input_action_t` structure:
  - `ik_input_action_type_t type`
  - `uint32_t codepoint` - For IK_INPUT_CHAR
- [x] Define `ik_input_parser_t` structure (opaque, for escape sequence buffering):
  - `char esc_buf[16]` - Escape sequence buffer
  - `size_t esc_len` - Current escape sequence length
  - `bool in_escape` - Currently parsing escape sequence
- [x] Add function declarations:
  - `res_t ik_input_parser_create(void *parent, ik_input_parser_t **parser_out)`
  - `res_t ik_input_parse_byte(ik_input_parser_t *parser, char byte, ik_input_action_t *action_out)`

### Step 2: Input Parser Creation ✅ COMPLETE

- [x] Create `src/input.c` implementation
- [x] Implement `ik_input_parser_create()`:
  - Allocate parser with talloc
  - Initialize fields (esc_len = 0, in_escape = false)
- [x] Write test `test_input_parser_create()` in `tests/unit/input_test.c`:
  - Create parser
  - Verify successful allocation
- [x] Write test `test_input_parser_create_oom()`:
  - Test OOM scenario
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 3: Parse Regular Characters ✅ COMPLETE

- [x] Implement `ik_input_parse_byte()` for regular ASCII characters:
  - If byte is printable ASCII (0x20-0x7E), return IK_INPUT_CHAR
  - Set action_out->codepoint to byte value
- [x] Write test `test_input_parse_regular_char()`:
  - Parse 'a', verify IK_INPUT_CHAR with codepoint 'a'
  - Parse 'Z', verify IK_INPUT_CHAR with codepoint 'Z'
  - Parse '5', verify IK_INPUT_CHAR with codepoint '5'
- [x] Write test `test_input_parse_nonprintable()`:
  - Parse bytes outside printable ASCII range (0x01, 0x7F)
  - Verify IK_INPUT_UNKNOWN returned
- [x] Write assertion tests for NULL parameters
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Parse Control Characters ✅ COMPLETE

- [x] Implement parsing for control characters:
  - `\n` (0x0A) → IK_INPUT_NEWLINE
  - `\x7F` (DEL) → IK_INPUT_BACKSPACE
  - `\x03` (Ctrl+C) → IK_INPUT_CTRL_C
- [x] Write test `test_input_parse_newline()`:
  - Parse '\n', verify IK_INPUT_NEWLINE
- [x] Write test `test_input_parse_backspace()`:
  - Parse 0x7F, verify IK_INPUT_BACKSPACE
- [x] Write test `test_input_parse_ctrl_c()`:
  - Parse 0x03, verify IK_INPUT_CTRL_C
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Parse Escape Sequences (Arrow Keys) ✅ COMPLETE

- [x] Implement escape sequence parsing:
  - Detect ESC (0x1B) and enter escape mode
  - Buffer subsequent bytes in esc_buf
  - Recognize complete sequences:
    - `\x1b[A` → IK_INPUT_ARROW_UP
    - `\x1b[B` → IK_INPUT_ARROW_DOWN
    - `\x1b[C` → IK_INPUT_ARROW_RIGHT
    - `\x1b[D` → IK_INPUT_ARROW_LEFT
    - `\x1b[3~` → IK_INPUT_DELETE
  - Return IK_INPUT_UNKNOWN for incomplete sequences (need more bytes)
  - Reset parser state after complete or invalid sequence
- [x] Write test `test_input_parse_arrow_up()`:
  - Parse ESC, verify IK_INPUT_UNKNOWN (incomplete)
  - Parse '[', verify IK_INPUT_UNKNOWN (incomplete)
  - Parse 'A', verify IK_INPUT_ARROW_UP (complete)
- [x] Write test `test_input_parse_arrow_down()`:
  - Parse full sequence `\x1b[B`, verify IK_INPUT_ARROW_DOWN
- [x] Write test `test_input_parse_arrow_left()`:
  - Parse full sequence `\x1b[D`, verify IK_INPUT_ARROW_LEFT
- [x] Write test `test_input_parse_arrow_right()`:
  - Parse full sequence `\x1b[C`, verify IK_INPUT_ARROW_RIGHT
- [x] Write test `test_input_parse_delete()`:
  - Parse full sequence `\x1b[3~`, verify IK_INPUT_DELETE
- [x] Write test `test_input_parse_invalid_escape()`:
  - Parse ESC followed by invalid sequence
  - Verify parser resets and handles next input correctly
- [x] Added additional edge case tests for buffer overflow and incomplete sequences
- [x] Update Makefile to include input.c in build (already done in previous step)
- [x] Run quality gates: `make check`, `make lint`, `make coverage` - all pass with 100% coverage

**Status**: Escape sequence parsing complete with full test coverage. Used LCOV_EXCL_BR_LINE for 3 unreachable defensive branches.

### Step 6: UTF-8 Multi-byte Character Support ✅ COMPLETE

- [x] Extend `ik_input_parse_byte()` to handle UTF-8:
  - Detect UTF-8 lead bytes (0xC0-0xF7)
  - Buffer continuation bytes (0x80-0xBF)
  - Decode complete UTF-8 sequence into codepoint
  - Return IK_INPUT_CHAR with decoded codepoint
  - Return IK_INPUT_UNKNOWN for incomplete sequences
- [x] Write test `test_input_parse_utf8_2byte()`:
  - Parse é (0xC3 0xA9), verify IK_INPUT_CHAR with codepoint U+00E9
- [x] Write test `test_input_parse_utf8_3byte()`:
  - Parse ☃ (0xE2 0x98 0x83), verify IK_INPUT_CHAR with codepoint U+2603
- [x] Write test `test_input_parse_utf8_4byte()`:
  - Parse 🎉 (0xF0 0x9F 0x8E 0x89), verify IK_INPUT_CHAR with codepoint U+1F389
- [x] Write test `test_input_parse_utf8_incomplete()`:
  - Parse only lead byte, verify IK_INPUT_UNKNOWN (incomplete)
- [x] Write test `test_input_parse_utf8_invalid()`:
  - Parse invalid UTF-8 sequence
  - Verify parser resets correctly
- [x] Run quality gates: `make check`, `make lint`, `make coverage` - all pass with 100% coverage

### Step 7: Demo in client.c

- [x] Update `src/client.c` to demonstrate input parser:
  - Keep terminal initialization from previous demo
  - Add input parser creation
  - Main loop: read bytes, parse into actions, display action type and details
  - Example output: "CHAR: 'a' (U+0061)" or "ARROW_LEFT" or "CTRL_C"
  - Fixed Enter key to recognize both '\r' (carriage return) and '\n' (line feed)
  - Fixed parser freeze on unrecognized escape sequences (Insert key, etc.)
  - Refactored to reduce complexity below threshold (15)
  - Achieved 100% test coverage (217 checks, 0 failures)
  - Exit on Ctrl+C
- [x] Build and manually test:
  - `make && ./ikigai`
  - Type regular keys, verify they display as CHAR with codepoint
  - Try arrow keys, verify they parse correctly
  - Try UTF-8: type é, 🎉, verify correct codepoint display
  - Press Ctrl+C to exit
- [x] Commit work: "Demo input parser in client.c with full UTF-8 and escape sequence support"

### Step 8: Security Hardening - Fix UTF-8 Vulnerabilities ✅ COMPLETE

**Status**: All critical UTF-8 security vulnerabilities fixed with 100% test coverage.

**Files**: `src/input.c`, `tests/unit/input_pathological_test.c`

**Security Analysis**: See `SECURITY_ANALYSIS.md` for full details.

- [x] Create pathological test suite (`tests/unit/input_pathological_test.c`):
  - [x] 19 tests covering overlong encodings, surrogates, state confusion, boundaries, etc.
  - [x] All tests passing
- [x] Fix vulnerability #1: UTF-8 overlong encoding acceptance
  - [x] Add validation in `decode_utf8_sequence()` to reject 2-byte sequences with codepoint < 0x80
  - [x] Add validation to reject 3-byte sequences with codepoint < 0x800
  - [x] Add validation to reject 4-byte sequences with codepoint < 0x10000
  - [x] Return U+FFFD (replacement character) for overlong encodings
  - [x] Update tests: `test_utf8_overlong_2byte` expects rejection
  - [x] Update tests: `test_utf8_overlong_3byte_slash` expects rejection
  - [x] Update tests: `test_utf8_null_codepoint_overlong` expects rejection
  - [x] Write new test: `test_utf8_overlong_4byte` for 4-byte overlong
  - [x] Write new test: `test_utf8_valid_boundary_codepoints` (U+0080, U+0800, U+10000)
- [x] Fix vulnerability #2: UTF-16 surrogate acceptance
  - [x] Add validation in `decode_utf8_sequence()` to reject codepoints U+D800-U+DFFF
  - [x] Return U+FFFD for surrogate codepoints
  - [x] Update test: `test_utf8_surrogate_high` expects rejection
  - [x] Write new test: `test_utf8_surrogate_low` (U+DFFF)
- [x] Fix vulnerability #3: Out-of-range codepoint acceptance
  - [x] Add validation in `decode_utf8_sequence()` to reject codepoints > U+10FFFF
  - [x] Return U+FFFD for out-of-range codepoints
  - [x] Update test: `test_utf8_codepoint_too_large` expects rejection
  - [x] Write new test: `test_utf8_max_valid_codepoint` (U+10FFFF works correctly)
- [x] Add comprehensive validation test suite:
  - [x] Write test: `test_utf8_replacement_char_U_FFFD` (verify U+FFFD itself works)
  - [x] Write test: `test_utf8_valid_boundary_codepoints` (boundary edge cases)
  - [x] Write test: `test_utf8_max_valid_codepoint` (maximum valid codepoint)
  - [x] Write test: `test_utf8_surrogate_low` (low surrogate boundary)
  - [x] Write test: `test_utf8_overlong_4byte` (4-byte overlong coverage)
- [x] Makefile automatically includes `input_pathological_test` in `make check` (via wildcard)
- [x] Run quality gates: `make check`, `make lint`, `make coverage` - all pass
- [x] Verify coverage at 100% (lines, functions, branches)
- [x] Commit work: "Fix critical UTF-8 security vulnerabilities (overlong, surrogates, range)"

**References**:
- RFC 3629 (UTF-8 specification)
- Unicode Standard (surrogate and range definitions)
- CVE-2000-0884, CVE-2008-2938 (overlong encoding vulnerabilities)

---

## Task 3: Dynamic Zone Text Buffer

Create text buffer for workspace using `ik_byte_array_t`.

**Files**: `src/workspace.h`, `src/workspace.c`, `tests/unit/workspace_test.c`

### Step 1: Dynamic Zone Structure ✅

- [x] Create `src/workspace.h` header
- [x] Define `ik_workspace_t` structure:
  - `ik_byte_array_t *text` - UTF-8 text buffer
  - `size_t cursor_byte_offset` - Cursor position (byte offset)
- [x] Add function declarations:
  - `res_t ik_workspace_create(void *parent, ik_workspace_t **zone_out)`
  - `res_t ik_workspace_insert_codepoint(ik_workspace_t *zone, uint32_t codepoint)`
  - `res_t ik_workspace_insert_newline(ik_workspace_t *zone)`
  - `res_t ik_workspace_backspace(ik_workspace_t *zone)`
  - `res_t ik_workspace_delete(ik_workspace_t *zone)`
  - `res_t ik_workspace_get_text(ik_workspace_t *zone, char **text_out, size_t *len_out)`
  - `void ik_workspace_clear(ik_workspace_t *zone)`

### Step 2: Dynamic Zone Creation ✅

- [x] Create `src/workspace.c` implementation
- [x] Implement `ik_workspace_create()`:
  - Allocate zone with talloc
  - Create byte array for text
  - Initialize cursor_byte_offset to 0
- [x] Write test `test_workspace_create()` in `tests/unit/workspace_test.c`:
  - Create zone
  - Verify successful allocation
  - Verify text buffer is empty
  - Verify cursor at position 0
- [x] Write test `test_workspace_create_oom()`:
  - Test OOM scenarios
- [x] Write assertion tests for NULL parameters
- [x] Implement `ik_workspace_get_text()` and `ik_workspace_clear()` (needed for tests)
- [x] Update Makefile to include workspace.c in build
- [x] Run quality gates: `make check`, `make lint`, `make coverage` - all pass

### Step 2.5: Rename workspace to workspace ✅ COMPLETE

- [x] Rename source files:
  - `src/workspace.h` → `src/workspace.h`
  - `src/workspace.c` → `src/workspace.c`
  - `tests/unit/workspace_test.c` → `tests/unit/workspace_test.c`
- [x] Update all code references:
  - `ik_workspace_t` → `ik_workspace_t`
  - `ik_workspace_*()` functions → `ik_workspace_*()`
  - All internal variable names and comments
- [x] Update documentation:
  - Search and replace in `tasks.md`
  - Search and replace in all `docs/*.md` files
  - Update any other doc references (README, comments, etc.)
- [x] Update Makefile references
- [x] Run quality gates: `make check`, `make lint`, `make coverage`
- [x] Commit work: "Rename workspace to workspace for clarity"

### Step 3: Insert Codepoint at Cursor

- [ ] Implement `ik_workspace_insert_codepoint()`:
  - Encode codepoint to UTF-8 bytes
  - Insert bytes at cursor_byte_offset
  - Advance cursor_byte_offset by byte count
- [ ] Write test `test_workspace_insert_ascii()`:
  - Insert 'a', verify text is "a"
  - Insert 'b', verify text is "ab"
  - Verify cursor at end
- [ ] Write test `test_workspace_insert_utf8()`:
  - Insert é (U+00E9), verify correct UTF-8 encoding
  - Insert 🎉 (U+1F389), verify 4-byte encoding
- [ ] Write test `test_workspace_insert_middle()`:
  - Insert "ab", move cursor to 1, insert 'x'
  - Verify text is "axb"
  - Verify cursor at position 2
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Insert Newline at Cursor

- [ ] Implement `ik_workspace_insert_newline()`:
  - Insert '\n' byte at cursor_byte_offset
  - Advance cursor_byte_offset by 1
- [ ] Write test `test_workspace_insert_newline()`:
  - Insert "hello", insert newline, insert "world"
  - Verify text is "hello\nworld"
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Backspace (Delete Before Cursor)

- [ ] Implement `ik_workspace_backspace()`:
  - If cursor_byte_offset == 0, return success (no-op)
  - Find start of previous UTF-8 character
  - Delete bytes from previous char start to cursor
  - Update cursor_byte_offset to previous char start
- [ ] Write test `test_workspace_backspace_ascii()`:
  - Insert "abc", backspace once
  - Verify text is "ab", cursor at 2
- [ ] Write test `test_workspace_backspace_utf8()`:
  - Insert "a" + é (2 bytes) + "b", backspace once
  - Verify é deleted (both bytes), text is "ab"
- [ ] Write test `test_workspace_backspace_emoji()`:
  - Insert 🎉 (4 bytes), backspace once
  - Verify all 4 bytes deleted, text is empty
- [ ] Write test `test_workspace_backspace_at_start()`:
  - Empty buffer, backspace
  - Verify no-op, no error
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 6: Delete (Delete After Cursor)

- [ ] Implement `ik_workspace_delete()`:
  - If cursor at end of text, return success (no-op)
  - Find end of current UTF-8 character
  - Delete bytes from cursor to end of char
  - cursor_byte_offset stays same
- [ ] Write test `test_workspace_delete_ascii()`:
  - Insert "abc", move cursor to 0, delete once
  - Verify text is "bc", cursor still at 0
- [ ] Write test `test_workspace_delete_utf8()`:
  - Insert "a" + é + "b", move cursor to 1, delete once
  - Verify é deleted, text is "ab"
- [ ] Write test `test_workspace_delete_emoji()`:
  - Insert 🎉, move cursor to 0, delete once
  - Verify all 4 bytes deleted
- [ ] Write test `test_workspace_delete_at_end()`:
  - Insert "abc", cursor at end, delete
  - Verify no-op, no error
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 7: Get Text and Clear

- [ ] Implement `ik_workspace_get_text()`:
  - Return pointer to text buffer contents
  - Return length via out parameter
- [ ] Implement `ik_workspace_clear()`:
  - Clear byte array
  - Reset cursor_byte_offset to 0
- [ ] Write test `test_workspace_get_text()`:
  - Insert "hello", get text
  - Verify returned text matches
- [ ] Write test `test_workspace_clear()`:
  - Insert "hello", clear, verify empty
  - Verify cursor at 0
- [ ] Update Makefile to include workspace.c in build
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 8: Demo in client.c

- [ ] Update `src/client.c` to demonstrate workspace:
  - Keep terminal and input parser from previous demo
  - Add workspace creation
  - Main loop: parse input actions, apply to workspace
    - CHAR → insert codepoint
    - BACKSPACE → backspace
    - DELETE → delete
    - NEWLINE → insert newline
  - After each action, display buffer contents and cursor position
  - Example: "Buffer: 'hello' | Cursor: 5"
- [ ] Build and manually test:
  - `make && ./ikigai`
  - Type text, verify buffer updates
  - Use backspace/delete, verify correct deletion
  - Type UTF-8 characters, verify proper storage
  - Press Ctrl+C to exit
- [ ] Commit work: "Implement workspace text buffer with UTF-8 support"

---

## Task 4: Cursor Management with Grapheme Cluster Support

Create cursor manager for tracking both byte and grapheme offsets.

**Files**: `src/cursor.h`, `src/cursor.c`, `tests/unit/cursor_test.c`

**Dependencies**: libutf8proc (for grapheme cluster detection)

### Step 1: Cursor Structure and Creation

- [ ] Create `src/cursor.h` header
- [ ] Define `ik_cursor_t` structure:
  - `size_t byte_offset` - Byte position in UTF-8 string
  - `size_t grapheme_offset` - Grapheme cluster count from start
- [ ] Add function declarations:
  - `res_t ik_cursor_create(void *parent, ik_cursor_t **cursor_out)`
  - `res_t ik_cursor_set_position(ik_cursor_t *cursor, const char *text, size_t text_len, size_t byte_offset)`
  - `res_t ik_cursor_move_left(ik_cursor_t *cursor, const char *text, size_t text_len)`
  - `res_t ik_cursor_move_right(ik_cursor_t *cursor, const char *text, size_t text_len)`
  - `res_t ik_cursor_get_position(ik_cursor_t *cursor, size_t *byte_offset_out, size_t *grapheme_offset_out)`
- [ ] Create `src/cursor.c` implementation
- [ ] Implement `ik_cursor_create()`:
  - Allocate cursor with talloc
  - Initialize both offsets to 0
- [ ] Write test `test_cursor_create()` in `tests/unit/cursor_test.c`:
  - Create cursor, verify offsets are 0
- [ ] Write test `test_cursor_create_oom()`:
  - Test OOM scenario
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 2: Set Cursor Position

- [ ] Implement `ik_cursor_set_position()`:
  - Assert byte_offset <= text_len
  - Set cursor->byte_offset
  - Count grapheme clusters from start to byte_offset using libutf8proc
  - Set cursor->grapheme_offset
- [ ] Write test `test_cursor_set_position_ascii()`:
  - Text "hello", set position to byte 3
  - Verify byte_offset = 3, grapheme_offset = 3
- [ ] Write test `test_cursor_set_position_utf8()`:
  - Text "aéb" (4 bytes: a + C3 A9 + b)
  - Set position to byte 3 (after é)
  - Verify byte_offset = 3, grapheme_offset = 2
- [ ] Write test `test_cursor_set_position_emoji()`:
  - Text "a🎉b" (6 bytes: a + F0 9F 8E 89 + b)
  - Set position to byte 5 (after 🎉)
  - Verify byte_offset = 5, grapheme_offset = 2
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 3: Move Left by Grapheme Cluster

- [ ] Implement `ik_cursor_move_left()`:
  - If cursor at start (byte_offset == 0), return success (no-op)
  - Use libutf8proc to find previous grapheme boundary
  - Update both byte_offset and grapheme_offset
- [ ] Write test `test_cursor_move_left_ascii()`:
  - Text "abc", cursor at end
  - Move left, verify byte_offset = 2, grapheme_offset = 2
  - Move left, verify byte_offset = 1, grapheme_offset = 1
- [ ] Write test `test_cursor_move_left_utf8()`:
  - Text "aéb", cursor at end (byte 4)
  - Move left, verify moves to byte 1 (skips both bytes of é)
  - Verify grapheme_offset = 1
- [ ] Write test `test_cursor_move_left_emoji()`:
  - Text "a🎉", cursor at end (byte 5)
  - Move left, verify moves to byte 1 (skips all 4 bytes of 🎉)
  - Verify grapheme_offset = 1
- [ ] Write test `test_cursor_move_left_combining()`:
  - Text "e\u0301" (e + combining acute accent = é)
  - Cursor at end, move left
  - Verify moves to byte 0 (treats as single grapheme)
  - Verify grapheme_offset = 0
- [ ] Write test `test_cursor_move_left_at_start()`:
  - Cursor at start, move left
  - Verify no-op, stays at 0
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Move Right by Grapheme Cluster

- [ ] Implement `ik_cursor_move_right()`:
  - If cursor at end (byte_offset == text_len), return success (no-op)
  - Use libutf8proc to find next grapheme boundary
  - Update both byte_offset and grapheme_offset
- [ ] Write test `test_cursor_move_right_ascii()`:
  - Text "abc", cursor at start
  - Move right, verify byte_offset = 1, grapheme_offset = 1
- [ ] Write test `test_cursor_move_right_utf8()`:
  - Text "aéb", cursor at byte 1
  - Move right, verify byte_offset = 3 (skips both bytes of é)
  - Verify grapheme_offset = 2
- [ ] Write test `test_cursor_move_right_emoji()`:
  - Text "a🎉", cursor at byte 1
  - Move right, verify byte_offset = 5 (skips all 4 bytes)
  - Verify grapheme_offset = 2
- [ ] Write test `test_cursor_move_right_combining()`:
  - Text "e\u0301b", cursor at start
  - Move right, verify moves to byte 3 (skips e + combining)
  - Verify grapheme_offset = 1
- [ ] Write test `test_cursor_move_right_at_end()`:
  - Cursor at end, move right
  - Verify no-op, stays at end
- [ ] Update Makefile to include cursor.c in build
- [ ] Link with libutf8proc (-lutf8proc)
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Integration with Dynamic Zone

- [ ] Update `src/workspace.h` to include `ik_cursor_t *cursor` field
- [ ] Update `ik_workspace_create()` to create cursor
- [ ] Update all workspace operations to keep cursor in sync:
  - `ik_workspace_insert_codepoint()` - update cursor after insert
  - `ik_workspace_insert_newline()` - update cursor after insert
  - `ik_workspace_backspace()` - update cursor after delete
  - `ik_workspace_delete()` - cursor stays same
- [ ] Add new functions:
  - `res_t ik_workspace_cursor_left(ik_workspace_t *zone)`
  - `res_t ik_workspace_cursor_right(ik_workspace_t *zone)`
  - `res_t ik_workspace_get_cursor_position(ik_workspace_t *zone, size_t *byte_out, size_t *grapheme_out)`
- [ ] Update tests in `tests/unit/workspace_test.c`:
  - Verify cursor position after each operation
  - Test cursor movement with various UTF-8 content
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 6: Demo in client.c

- [ ] Update `src/client.c` to demonstrate cursor with grapheme support:
  - Keep previous components (terminal, input parser, workspace)
  - Add arrow key handling:
    - ARROW_LEFT → move cursor left (by grapheme)
    - ARROW_RIGHT → move cursor right (by grapheme)
  - Display both byte and grapheme positions
  - Example: "Buffer: 'a🎉b' | Byte: 5, Grapheme: 2"
- [ ] Build and manually test:
  - `make && ./ikigai`
  - Type text with emoji and UTF-8 characters
  - Use arrow keys to move cursor
  - Verify cursor moves by whole grapheme clusters
  - Try combining characters (e + ´), verify treated as single unit
  - Insert text in middle, verify correct behavior
  - Press Ctrl+C to exit
- [ ] Commit work: "Implement cursor management with grapheme cluster support"

---

## Task 5: Rendering Module (vterm integration)

Create rendering module using libvterm.

**Files**: `src/render.h`, `src/render.c`, `tests/unit/render_test.c`

**Dependencies**: libvterm

### Step 1: Render Context Structure

- [ ] Create `src/render.h` header
- [ ] Define `ik_render_ctx_t` structure:
  - `VTerm *vterm` - Virtual terminal
  - `VTermScreen *vscreen` - Screen interface
  - `int rows` - Screen height
  - `int cols` - Screen width
- [ ] Add function declarations:
  - `res_t ik_render_create(void *parent, int rows, int cols, ik_render_ctx_t **render_out)`
  - `void ik_render_clear(ik_render_ctx_t *render)`
  - `res_t ik_render_write_text(ik_render_ctx_t *render, const char *text, size_t len)`
  - `res_t ik_render_set_cursor(ik_render_ctx_t *render, int row, int col)`
  - `res_t ik_render_blit(ik_render_ctx_t *render, int tty_fd)`

### Step 2: Render Context Creation

- [ ] Create `src/render.c` implementation
- [ ] Implement `ik_render_create()`:
  - Allocate render context with talloc
  - Create VTerm with vterm_new(rows, cols)
  - Get VTermScreen with vterm_obtain_screen()
  - Set up talloc destructor to call vterm_free()
  - Initialize rows and cols
- [ ] Write test `test_render_create()` in `tests/unit/render_test.c`:
  - Create render context (80x24)
  - Verify successful allocation
  - Verify vterm created
- [ ] Write test `test_render_create_oom()`:
  - Test OOM scenario
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 3: Clear Screen

- [ ] Implement `ik_render_clear()`:
  - Call vterm_screen_reset(vscreen, 1) to clear
- [ ] Write test `test_render_clear()`:
  - Create render context
  - Write some text
  - Clear
  - Verify vterm is empty (read cells, verify all spaces)
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Write Text to vterm

- [ ] Implement `ik_render_write_text()`:
  - Write UTF-8 text to vterm using vterm_input_write()
  - Let vterm handle wrapping and cursor advancement
- [ ] Write test `test_render_write_text_ascii()`:
  - Write "hello"
  - Read back cells from vterm
  - Verify text appears at expected position
- [ ] Write test `test_render_write_text_utf8()`:
  - Write "héllo" (with é)
  - Verify UTF-8 handled correctly
- [ ] Write test `test_render_write_text_newline()`:
  - Write "line1\nline2"
  - Verify appears on two rows
- [ ] Write test `test_render_write_text_wrapping()`:
  - Create narrow vterm (10 cols)
  - Write text longer than width
  - Verify wraps to next line
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Set Cursor Position

- [ ] Implement `ik_render_set_cursor()`:
  - Use vterm_state_get_cursorpos() to set cursor
  - Or use escape sequence "\x1b[row;colH"
- [ ] Write test `test_render_set_cursor()`:
  - Set cursor to (5, 10)
  - Read cursor position from vterm
  - Verify position matches
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 6: Blit vterm to Screen

- [ ] Implement `ik_render_blit()`:
  - Build frame buffer in memory:
    - Home cursor: "\x1b[H"
    - For each cell, write UTF-8 character
    - Position cursor at final location
  - Single write() call to tty_fd
- [ ] Write test `test_render_blit()`:
  - Note: This is hard to fully unit test
  - Create render context
  - Write text
  - Create mock file descriptor (pipe)
  - Blit to pipe
  - Read from pipe, verify output contains expected sequences
- [ ] Update Makefile to include render.c in build
- [ ] Link with libvterm (-lvterm)
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 7: Demo in client.c

- [ ] Update `src/client.c` to demonstrate vterm rendering:
  - Keep all previous components
  - Add render context creation (use terminal dimensions)
  - On each input action:
    - Apply action to workspace
    - Clear render context
    - Write workspace text to render context
    - Calculate cursor screen position (accounting for wrapping)
    - Set cursor in render context
    - Blit to screen
  - This creates a live-updating terminal display!
- [ ] Build and manually test:
  - `make && ./ikigai`
  - Verify text appears on screen as you type
  - Verify cursor moves correctly
  - Verify text wraps at terminal width
  - Verify multi-line text displays correctly
  - Type long text, verify wrapping works
  - Type emoji and UTF-8, verify rendering correct
  - Press Ctrl+C to exit
- [ ] Commit work: "Implement vterm rendering with live display"

---

## Task 6: Main REPL Context and Event Loop

Integrate all modules into main REPL.

**Files**: `src/repl.h`, `src/repl.c`, `src/main.c`

### Step 1: REPL Context Structure

- [ ] Create `src/repl.h` header
- [ ] Define `ik_repl_ctx_t` structure:
  - `ik_term_ctx_t *term` - Terminal context
  - `ik_render_ctx_t *render` - Render context
  - `ik_workspace_t *workspace` - Workspace
  - `ik_input_parser_t *input_parser` - Input parser
  - `bool quit` - Exit flag
- [ ] Add function declarations:
  - `res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)`
  - `void ik_repl_cleanup(ik_repl_ctx_t *repl)`
  - `res_t ik_repl_run(ik_repl_ctx_t *repl)`

### Step 2: REPL Initialization

- [ ] Create `src/repl.c` implementation
- [ ] Implement `ik_repl_init()`:
  - Allocate repl context with talloc
  - Initialize terminal (raw mode, alternate screen)
  - Get terminal dimensions
  - Initialize render context with terminal dimensions
  - Initialize workspace
  - Initialize input parser
  - Set quit flag to false
- [ ] Write integration test `test_repl_init()` in `tests/integration/repl_test.c`:
  - Note: May need to skip if no TTY available
  - Initialize REPL
  - Verify all components created
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 3: REPL Cleanup

- [ ] Implement `ik_repl_cleanup()`:
  - Call terminal cleanup
  - Note: Other components cleaned up via talloc hierarchy
- [ ] Write test `test_repl_cleanup()`:
  - Initialize REPL, then cleanup
  - Verify terminal restored
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Render Frame

- [ ] Add helper function `ik_repl_render_frame()`:
  - Clear render context
  - Get text from workspace
  - Write text to render context
  - Get cursor position from workspace
  - Calculate screen position for cursor (account for wrapping)
  - Set cursor in render context
  - Blit to screen
- [ ] This will primarily be tested via manual testing
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Process Input Action

- [ ] Add helper function `ik_repl_process_action()`:
  - Handle each action type:
    - `IK_INPUT_CHAR` → insert codepoint into workspace
    - `IK_INPUT_NEWLINE` → insert newline into workspace
    - `IK_INPUT_BACKSPACE` → backspace in workspace
    - `IK_INPUT_DELETE` → delete in workspace
    - `IK_INPUT_ARROW_LEFT` → move cursor left
    - `IK_INPUT_ARROW_RIGHT` → move cursor right
    - `IK_INPUT_ARROW_UP` → (defer to Phase 2, no-op for now)
    - `IK_INPUT_ARROW_DOWN` → (defer to Phase 2, no-op for now)
    - `IK_INPUT_CTRL_C` → set quit flag
    - `IK_INPUT_UNKNOWN` → ignore
- [ ] Write tests for each action type
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 6: Main Event Loop

- [ ] Implement `ik_repl_run()`:
  - Initial render
  - Loop until quit:
    - Read bytes from terminal (blocking read)
    - For each byte:
      - Parse byte with input parser
      - Process resulting action
    - Render frame after processing input
- [ ] This is primarily integration/manual testing
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 7: Main Entry Point

- [ ] Rename `src/client.c` to `src/main.c` (preserve demo work as basis)
- [ ] Refactor `main()` to use REPL context:
  - Initialize REPL
  - Run REPL
  - Cleanup REPL
  - Return 0 on success, 1 on error
- [ ] Update Makefile: `src/main.c` builds to `ikigai` executable
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 8: Final Demo and Polish

- [ ] Build and comprehensive manual test:
  - `make && ./ikigai`
  - Test all features from manual testing checklist (see below)
  - Verify proper cleanup on exit
- [ ] Clean up any debug output from earlier demos
- [ ] Ensure code follows project style
- [ ] Run `make fmt`
- [ ] Commit work: "Complete Phase 1 REPL with full integration"

---

## Final Quality Gates and Manual Testing

### Quality Gates

- [ ] All tasks complete with 100% test coverage
- [ ] `make check` passes (all tests)
- [ ] `make lint` passes (complexity under threshold)
- [ ] `make coverage` shows 100% coverage (Lines, Functions, Branches)
- [ ] Run `make fmt` before committing

### Manual Testing

Perform manual testing to validate the complete system:

- [ ] **Launch and basic operation**:
  - Launch `./ikigai`
  - Verify alternate screen activated
  - Type some text, verify it appears
  - Exit with Ctrl+C, verify terminal restored cleanly

- [ ] **UTF-8 handling**:
  - Type emoji: 🎉 👨‍👩‍👧‍👦
  - Verify they display correctly
  - Type combining characters: e + ´ = é
  - Use left/right arrows through multi-byte characters
  - Verify cursor moves by whole grapheme clusters

- [ ] **Text editing**:
  - Type "hello world"
  - Move cursor to middle with arrow keys
  - Insert characters in middle
  - Backspace and delete characters
  - Verify editing works correctly

- [ ] **Multi-line input**:
  - Type text
  - Press Enter to insert newline
  - Continue typing on next line
  - Verify text wraps correctly
  - Use arrow keys to move around (left/right only, up/down no-op for now)

- [ ] **Edge cases**:
  - Fill entire screen with text
  - Verify wrapping continues to work
  - Try very long lines
  - Try rapid typing
  - Try holding down arrow keys

---

**Development Approach**: Strict TDD red/green cycle
1. Red: Write failing test first (verify it fails)
2. Green: Write minimal code to pass the test
3. Verify: Run `make check`, `make lint`, `make coverage`

**Zero Technical Debt**: Fix any deficiencies immediately as discovered.
