# Task: Use libxkbcommon to Translate CSI u Keyboard Events

## Target

Input Handling: Use libxkbcommon to translate CSI u keycode + modifiers into the correct Unicode character based on the user's keyboard layout.

## Model

Use Opus with extended thinking for this task.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/scm.md

## Pre-read Docs
- terminal-emulators/README.md (CSI u protocol details)
- https://xkbcommon.org/doc/current/group__state.html (libxkbcommon state API)
- https://sw.kovidgoyal.net/kitty/keyboard-protocol/ (CSI u specification)

## Pre-read Source
- src/input.h (ik_input_action_type_t, ik_input_parser_t)
- src/input.c (ik_input_parse_byte, parse_csi_u_sequence)
- src/terminal.h (ik_term_ctx_t)
- src/terminal.c (CSI u enable/disable)
- tests/unit/input/escape_test.c (existing CSI u tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- libxkbcommon-dev is installed (`dpkg -l | grep libxkbcommon-dev`)

## Background

### Problem

The CSI u keyboard protocol sends the **base key** with modifiers, not the translated character. Per the Kitty keyboard protocol specification:

> "the codepoint used is _always_ the lower-case (or more technically, un-shifted) version of the key"

So when a user presses Shift+= to type `+`:
- Terminal sends: `ESC [ 61 ; 2 u` (keycode 61 = `=`, modifiers 2 = Shift)
- Current parser outputs: `=` (wrong)
- Expected output: `+`

This affects all shifted characters (`!@#$%^&*()_+{}|:"<>?` etc.) and breaks on non-US keyboard layouts.

### Solution

Use libxkbcommon to translate keycode + modifiers into the correct Unicode character based on the user's configured keyboard layout.

libxkbcommon is the standard library used by X11/Wayland for keyboard handling. It:
- Loads the system's configured keyboard layout
- Translates keycode + modifier state → Unicode character
- Returns 0 for non-printable keys (Enter, Escape, arrows, etc.)

### Integration Points

1. **Initialization**: Load keyboard layout via `xkb_context_new()` and `xkb_keymap_new_from_names()`
2. **State management**: Create `xkb_state` and update modifier state
3. **Translation**: Call `xkb_state_key_get_utf32()` to get the character
4. **Fallback**: If result is 0, fall through to existing special key handling

### Key Insight

The CSI u keycode is a Unicode codepoint (e.g., 61 for `=`), but libxkbcommon expects X11 scancodes. Research needed on:
- How to map Unicode codepoint back to X11 keycode, OR
- Whether to use `xkb_keysym_to_utf32()` with the keysym instead

Alternatively, consider using CSI u flag 4 (alternate key reporting) which makes the terminal include the shifted character in the sequence. Compare complexity of both approaches.

## Expected Behavior

| Input | CSI u Sequence | Current Output | Expected Output |
|-------|----------------|----------------|-----------------|
| `+` (Shift+=) | `ESC[61;2u` | `=` | `+` |
| `!` (Shift+1) | `ESC[49;2u` | `1` | `!` |
| `A` (Shift+a) | `ESC[97;2u` | `a` | `A` |
| Enter | `ESC[13;1u` | IK_INPUT_NEWLINE | IK_INPUT_NEWLINE (unchanged) |
| Shift+Enter | `ESC[13;2u` | IK_INPUT_INSERT_NEWLINE | IK_INPUT_INSERT_NEWLINE (unchanged) |
| Ctrl+C | `ESC[99;5u` | IK_INPUT_CTRL_C | IK_INPUT_CTRL_C (unchanged) |
| Arrow Up | `ESC[A` | IK_INPUT_ARROW_UP | IK_INPUT_ARROW_UP (unchanged) |
| Mouse scroll | `ESC[<64;...M` | IK_INPUT_SCROLL_UP | IK_INPUT_SCROLL_UP (unchanged) |

## Implementation Strategy

1. **Research phase**: Determine mapping from CSI u keycode to libxkbcommon input
2. **Add libxkbcommon dependency**: Update Makefile to link `-lxkbcommon`
3. **Initialize xkb state**: Create xkb_context, xkb_keymap, xkb_state at terminal init
4. **Modify CSI u parser**:
   - After parsing keycode + modifiers
   - Update xkb_state with modifier flags
   - Call translation function
   - If result != 0: return IK_INPUT_CHAR with that codepoint
   - If result == 0: fall through to existing special key handling
5. **Cleanup**: Free xkb resources at terminal cleanup

## TDD Cycle

### Red

Add tests for shifted characters:

```c
// Test: CSI u Shift+= produces '+' on US layout
START_TEST(test_csi_u_shift_equals_produces_plus)
{
    // ESC[61;2u = '=' with Shift modifier
    // Should produce '+' after xkbcommon translation
}
END_TEST

// Test: CSI u Shift+a produces 'A'
START_TEST(test_csi_u_shift_a_produces_uppercase)
{
    // ESC[97;2u = 'a' with Shift modifier
    // Should produce 'A' after xkbcommon translation
}
END_TEST
```

### Green

Implement libxkbcommon integration.

### Verify

1. `make check` - all tests pass
2. `make lint` - no issues
3. Manual test: Type `1 + 1` and verify it appears correctly
4. Test shifted characters: `!@#$%^&*()_+`
5. Verify special keys still work: Enter, Shift+Enter, Ctrl+C, arrows, mouse scroll

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- Shifted characters produce correct output on user's keyboard layout
- All existing special key handling preserved (Enter, Ctrl+C, arrows, mouse, etc.)
- libxkbcommon linked in Makefile

## Notes

- libxkbcommon is already installed on this system
- The translation should be locale/layout aware - test with US layout first
- Non-printable keys (Enter, arrows, etc.) return 0 from xkb_state_key_get_utf32()
- Consider caching the xkb_state to avoid repeated initialization
- Mouse events and non-CSI-u sequences should be completely unaffected
