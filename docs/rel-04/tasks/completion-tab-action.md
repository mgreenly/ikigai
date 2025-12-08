# Task: Add TAB Input Action

## Target
Feature: Tab Completion - Input Action

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/input.h (ik_input_action_type_t enum, existing actions)
- src/input.c (ik_input_parse_byte, escape sequence parsing)

### Pre-read Tests (patterns)
- tests/unit/input/parser_test.c (input action parsing tests)

## Pre-conditions
- `make check` passes
- Input parser handles all existing key combinations
- Tab character (0x09) is currently handled as regular character

## Task
Add `IK_INPUT_TAB` action to input action enum and modify input parser to recognize Tab key (byte 0x09) as a distinct action instead of treating it as a regular character.

This is a prerequisite for tab completion - the parser needs to emit a TAB action that can be handled separately from character insertion.

## TDD Cycle

### Red
1. Add `IK_INPUT_TAB` to enum in `src/input.h`:
   ```c
   typedef enum {
       IK_INPUT_CHAR,
       // ... existing actions ...
       IK_INPUT_CTRL_W,
       IK_INPUT_TAB,        // Tab key (completion trigger)
       IK_INPUT_UNKNOWN
   } ik_input_action_type_t;
   ```

2. Add test to `tests/unit/input/parser_test.c`:
   ```c
   START_TEST(test_parse_tab_key)
   {
       ik_input_parser_t *parser = ik_input_parser_create(NULL);
       ik_input_action_t action;

       // Tab is byte 0x09
       ik_input_parse_byte(parser, '\t', &action);

       ck_assert_int_eq(action.type, IK_INPUT_TAB);
       ck_assert_int_eq(action.codepoint, 0);  // No codepoint for TAB

       talloc_free(parser);
   }
   END_TEST
   ```

3. Run `make check` - expect test failure (TAB not recognized yet)

### Green
1. Modify `ik_input_parse_byte()` in `src/input.c`:
   ```c
   // Add before UTF-8 handling, after escape sequence handling
   // Tab key (0x09) - completion trigger
   if (byte == '\t') {
       action_out->type = IK_INPUT_TAB;
       action_out->codepoint = 0;
       return;
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Ensure TAB is handled before any UTF-8 or character logic
2. Verify TAB is not treated as whitespace character elsewhere
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- IK_INPUT_TAB action exists in enum
- Input parser recognizes Tab key (0x09) as IK_INPUT_TAB
- Tab is no longer inserted as regular character
- Existing tests still pass
- 100% test coverage maintained
