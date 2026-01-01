# Fix Dead Code Tests

**UNATTENDED EXECUTION:** This task runs automatically. Do not ask for confirmation.

## Context

The function `{{function}}` was removed from `{{file}}`.
This broke the following test files (they fail to compile):

{{broken_tests}}

These tests are testing dead code and need to be fixed or removed.

## Task

Fix the broken test files so they compile and pass.

## Rules

- ONLY modify test files listed above
- Do NOT modify production source files (src/)
- Remove test functions that solely test `{{function}}`
- If a test partially uses `{{function}}`, remove just that usage
- Remove any #include for headers that no longer exist

## Required Actions

1. Read each broken test file
2. Find references to `{{function}}`
3. Delete the test functions that call it
4. If deleting leaves an empty test suite, delete the entire file
5. Ensure remaining tests still compile

The harness verifies these tests compile and pass after you finish.
