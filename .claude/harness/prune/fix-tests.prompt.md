# Fix Dead Code Tests

**UNATTENDED EXECUTION:** This task runs automatically. Do not ask for confirmation.

## Context

The function `{{function}}` was removed from `{{file}}`.
This broke some tests. Those tests are testing dead code and need to be fixed or removed.

## Task

Fix or remove tests that reference `{{function}}`.

## Rules

- ONLY modify test files
- Do NOT modify production source files
- Remove test functions that solely test `{{function}}`
- If a test partially uses `{{function}}`, remove just that usage

The harness verifies tests pass after you finish.
