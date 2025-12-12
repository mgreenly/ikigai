# Task: Parse CSI u Enter Key Sequences

## Target

Input Handling: Parse CSI u sequences for modified Enter keys (Shift/Ctrl/Alt+Enter) and emit IK_INPUT_INSERT_NEWLINE.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/scm.md

## Pre-read Docs
- terminal-emulators/README.md (CSI u protocol details, modifier encoding)
- terminal-emulators/IMPLEMENTATION_GUIDE.md (implementation reference)

## Pre-read Source
- src/input.h (ik_input_action_type_t, ik_input_parser_t)
- src/input.c (ik_input_parse_byte, escape sequence parsing)
- tests/unit/input/input_test.c (existing input parser tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `csi-u-terminal-support.md` is complete (CSI u enabled in terminal)

## Background

With CSI u protocol enabled, modified keys arrive as:
```
ESC [ keycode ; modifiers u
```

For Enter (keycode 13):
- `ESC[13;1u` - Plain Enter (modifiers=1, no mods)
- `ESC[13;2u` - Shift+Enter (modifiers=2, shift bit set)
- `ESC[13;5u` - Ctrl+Enter (modifiers=5, ctrl bit set)
- `ESC[13;3u` - Alt+Enter (modifiers=3, alt bit set)

Modifier encoding: `modifiers = 1 + (shift=1 | alt=2 | ctrl=4 | ...)`

All modified Enter keys should emit `IK_INPUT_INSERT_NEWLINE` (same as Ctrl+J).
Plain Enter (`ESC[13;1u`) should emit `IK_INPUT_NEWLINE` (submit).

Additionally, Alacritty with flag 8+ sends modifier-only key events with keycodes > 50000 (e.g., Shift alone = 57441). These must be filtered and ignored.

## Expected Behavior

| Sequence | Key | Action Type |
|----------|-----|-------------|
| `ESC[13;1u` | Enter | IK_INPUT_NEWLINE (submit) |
| `ESC[13;2u` | Shift+Enter | IK_INPUT_INSERT_NEWLINE |
| `ESC[13;5u` | Ctrl+Enter | IK_INPUT_INSERT_NEWLINE |
| `ESC[13;3u` | Alt+Enter | IK_INPUT_INSERT_NEWLINE |
| `ESC[13;6u` | Ctrl+Shift+Enter | IK_INPUT_INSERT_NEWLINE |
| `ESC[57441;...u` | Modifier-only | IK_INPUT_UNKNOWN (ignore) |

## TDD Cycle

### Red

Add tests to `tests/unit/input/input_test.c`:

```c
// Test: CSI u plain Enter emits NEWLINE (submit)
START_TEST(test_csi_u_plain_enter)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;1u = plain Enter
    const char seq[] = "\x1b[13;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);
}
END_TEST

// Test: CSI u Shift+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_shift_enter)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;2u = Shift+Enter
    const char seq[] = "\x1b[13;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);
}
END_TEST

// Test: CSI u Ctrl+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_ctrl_enter)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;5u = Ctrl+Enter
    const char seq[] = "\x1b[13;5u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);
}
END_TEST

// Test: CSI u Alt+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_alt_enter)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;3u = Alt+Enter
    const char seq[] = "\x1b[13;3u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);
}
END_TEST

// Test: CSI u Ctrl+Shift+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_ctrl_shift_enter)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;6u = Ctrl+Shift+Enter (1 + 1 + 4)
    const char seq[] = "\x1b[13;6u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);
}
END_TEST

// Test: Alacritty modifier-only events are ignored
START_TEST(test_csi_u_modifier_only_ignored)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[57441;2u = Shift key alone (Alacritty)
    const char seq[] = "\x1b[57441;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
}
END_TEST

// Test: Ctrl+J still works (not CSI u)
START_TEST(test_ctrl_j_still_works)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Ctrl+J = 0x0A (LF)
    ik_input_parse_byte(parser, 0x0A, &action);

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);
}
END_TEST
```

### Green

Update `src/input.c` to parse CSI u sequences:

1. Add a helper to parse CSI u format in the escape sequence handler:

```c
// Parse CSI u sequence: ESC [ keycode ; modifiers u
// Returns true if valid CSI u sequence parsed
static bool parse_csi_u_sequence(const ik_input_parser_t *parser,
                                  ik_input_action_t *action_out)
{
    // Minimum: ESC [ digit u = 4 chars in buffer (excluding ESC)
    // Format: [keycode;modifiers u
    if (parser->esc_len < 3) {
        return false;
    }

    // Must end with 'u'
    if (parser->esc_buf[parser->esc_len - 1] != 'u') {
        return false;
    }

    // Parse keycode and modifiers
    int32_t keycode = 0;
    int32_t modifiers = 1;  // Default: no modifiers

    size_t i = 1;  // Skip '[' (buf[0] is '[')

    // Parse keycode
    while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
        keycode = keycode * 10 + (parser->esc_buf[i] - '0');
        i++;
    }

    // Parse modifiers if present
    if (i < parser->esc_len && parser->esc_buf[i] == ';') {
        i++;
        modifiers = 0;
        while (i < parser->esc_len && parser->esc_buf[i] >= '0' && parser->esc_buf[i] <= '9') {
            modifiers = modifiers * 10 + (parser->esc_buf[i] - '0');
            i++;
        }
    }

    // Filter Alacritty modifier-only events (keycode > 50000)
    if (keycode > 50000) {
        action_out->type = IK_INPUT_UNKNOWN;
        return true;
    }

    // Handle Enter key (keycode 13)
    if (keycode == 13) {
        if (modifiers == 1) {
            // Plain Enter - submit
            action_out->type = IK_INPUT_NEWLINE;
        } else {
            // Modified Enter (Shift/Ctrl/Alt) - insert newline
            action_out->type = IK_INPUT_INSERT_NEWLINE;
        }
        return true;
    }

    // Other CSI u keys - not handled yet
    action_out->type = IK_INPUT_UNKNOWN;
    return true;
}
```

2. Call this helper in `handle_escape_sequence()` when sequence ends with 'u':

```c
// In handle_escape_sequence(), when byte == 'u':
if (byte == 'u') {
    parser->esc_buf[parser->esc_len++] = byte;
    parser->esc_buf[parser->esc_len] = '\0';

    if (parse_csi_u_sequence(parser, action_out)) {
        reset_escape_state(parser);
        return;
    }

    // Not a recognized CSI u sequence
    action_out->type = IK_INPUT_UNKNOWN;
    reset_escape_state(parser);
    return;
}
```

### Verify

1. `make check` - all tests pass
2. `make lint` - no issues
3. Manual test in CSI u terminal: Shift+Enter, Ctrl+Enter, Alt+Enter all insert newlines

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- CSI u Enter sequences parsed correctly
- Modified Enter keys emit IK_INPUT_INSERT_NEWLINE
- Plain CSI u Enter emits IK_INPUT_NEWLINE
- Alacritty modifier-only events filtered
- Ctrl+J continues to work as fallback

## Notes

- This task only handles Enter key. Other CSI u keys can be added later.
- The modifier bits allow detecting any combination (Ctrl+Shift+Alt+Enter, etc.)
- Terminals without CSI u support continue to use Ctrl+J for newline insertion
