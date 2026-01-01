# Delete Tests Using Dead Function

**UNATTENDED EXECUTION:** Do not ask questions. Just delete the tests.

## Context

The function `{{function}}` is dead code (not reachable from main). Tests that call this function are testing dead functionality and should be deleted.

## Task

Delete the following tests from `{{test_file}}`:
{{tests_to_delete}}

Also remove the corresponding `tcase_add_test` lines from the suite setup.

## Steps

1. Read `{{test_file}}`
2. Delete each START_TEST...END_TEST block for the listed tests
3. Delete the corresponding `tcase_add_test(tc_*, test_name)` lines
4. If a TCase becomes empty after deletion, delete it entirely

## Rules

- Use the Edit tool for all changes
- Only delete the specified tests, leave others intact
- Do NOT modify src/ files
- Do NOT create new files
