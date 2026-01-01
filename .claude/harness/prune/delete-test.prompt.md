# Delete Single Test

**UNATTENDED EXECUTION:** Do not ask questions. Just delete the test.

## Task

Delete test `{{test_name}}` from `{{test_file}}`:

1. Delete the `START_TEST({{test_name}}) { ... } END_TEST` block
2. Delete the `tcase_add_test(tc_*, {{test_name}})` line

## Rules

- Use the Edit tool
- Only delete this one test
- Do NOT touch other tests or TCase infrastructure
