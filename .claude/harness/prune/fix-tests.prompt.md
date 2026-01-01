# Fix Dead Code Tests

**UNATTENDED EXECUTION:** Do not ask questions. Just fix the code.

## Context

The function `{{function}}` was removed. These test files now fail to compile:

{{broken_tests}}

## Task

Delete all references to `{{function}}` from the broken test files.

## Steps

1. Read the broken test file
2. Use grep to find all lines containing `{{function}}`
3. For each test function that calls `{{function}}`:
   - Delete the entire test function (from START_TEST to END_TEST)
   - Delete the corresponding `tcase_add_test(..., test_name)` line
4. If ALL tests in a file are deleted, delete the entire file

## Critical

- Use the Edit tool to make changes
- Delete BOTH the test function AND its tcase_add_test registration
- Do NOT modify src/ files
