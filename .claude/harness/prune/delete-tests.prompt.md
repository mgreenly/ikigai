# Delete Tests Using Dead Function

**UNATTENDED EXECUTION:** Do not ask questions. Just delete the tests.

## Context

The function `{{function}}` is dead code (not reachable from main). Tests that call this function are testing dead functionality and should be deleted.

## Task

Delete the following tests from `{{test_file}}`:
{{tests_to_delete}}

## Steps

1. Read `{{test_file}}`
2. For each test in the list above:
   - Delete the `START_TEST(test_name) { ... } END_TEST` block
   - Delete the corresponding `tcase_add_test(tc_*, test_name)` line

## Rules

- Use the Edit tool for all changes
- Only delete the specified tests, leave others intact
- Do NOT delete TCase infrastructure (tcase_create, tcase_set_timeout, etc.) - a later phase handles that
- Do NOT modify src/ files
- Do NOT create new files
