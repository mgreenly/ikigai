# Refactor Tests to Remove Function Call

**UNATTENDED EXECUTION:** Do not ask questions. Just refactor the tests.

## Context

The function `{{function}}` is dead code (not reachable from main). Some tests call this function but may not actually need it.

## Task

Refactor the following tests in `{{test_file}}` to NOT call `{{function}}`:
{{tests_to_refactor}}

## Approach

For each test, determine:
1. Is the test TESTING the function itself? → Cannot refactor, report failure
2. Is the test USING the function for setup/state? → Inline the logic or find alternative

If the test is testing the dead function directly (e.g., asserting on its return value), you cannot refactor it - the test should be deleted instead.

If the test uses the function incidentally, refactor to:
- Inline the function's logic
- Use an alternative approach
- Remove the unnecessary call

## Steps

1. Read `{{test_file}}`
2. For each listed test, analyze how `{{function}}` is used
3. Refactor to eliminate the function call where possible
4. If a test cannot be refactored (it's testing the function), leave it unchanged

## Rules

- Use the Edit tool for all changes
- Do NOT modify src/ files
- Do NOT create new files
- Do NOT create helpers
