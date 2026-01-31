## Objective

All unit tests must pass.

## Strategy

1. Run `.claude/scripts/check-unit` to identify failing tests
2. For each file with failing tests:
   - Use `.claude/scripts/check-unit --file=<path>` to see all test failures for that file
   - Read the test assertions and failure messages
   - Understand what behavior the tests expect
   - Fix the implementation to match expected behavior
   - **Verify `.claude/scripts/check-unit` returns `{"ok": true}` after changes**
3. Continue until all unit tests pass

## Guidelines

- Understand the test intent before changing code
- Do not modify tests to make them pass (fix the implementation instead)
- Ensure fixes don't break other passing tests
- Maintain existing API contracts and behavior

## Common Issues

- **Assertion failures**: Implementation doesn't match expected behavior
- **Segfaults**: Memory access errors, null pointer dereferences
- **Talloc errors**: Ownership violations, memory leaks
- **Setup/teardown issues**: Test fixtures not properly initialized/cleaned

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For test structure and fixtures: `/load tdd`
- For database fixtures: `/load database`

## Acceptance

DONE when `.claude/scripts/check-unit` returns `{"ok": true}`
