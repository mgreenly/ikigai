# Cleanup Empty TCase

**UNATTENDED EXECUTION:** Do not ask questions. Just delete the empty TCase.

## Context

Tests have been deleted from `{{test_file}}`. The TCase `{{tcase_name}}` now has no tests (no `tcase_add_test` calls for it).

## Task

Delete all infrastructure for the empty TCase `{{tcase_name}}`:

1. The variable declaration: `TCase *{{tcase_name}} = tcase_create(...);`
2. All configuration calls: `tcase_set_timeout({{tcase_name}}, ...);`
3. All fixture calls: `tcase_add_unchecked_fixture({{tcase_name}}, ...);` or `tcase_add_checked_fixture({{tcase_name}}, ...);`
4. The suite registration: `suite_add_tcase(s, {{tcase_name}});`

## Steps

1. Read `{{test_file}}`
2. Delete each line that references `{{tcase_name}}`

## Rules

- Use the Edit tool for all changes
- Only delete lines for the specified TCase
- Do NOT delete other TCases or their tests
- Do NOT modify src/ files
