# Delete Tests for Removed Function

**UNATTENDED EXECUTION:** Do not ask questions. Just delete the tests.

## Context

The function `{{function}}` was removed from the codebase. The following tests in `{{test_file}}` call this function and must be deleted:

{{tests_to_delete}}

## Task

Delete ONLY these specific tests from the file. Do NOT modify any other tests.

## Steps

1. Read `{{test_file}}`
2. For EACH test listed above, delete TWO things:
   - The entire test block: from `START_TEST(test_name)` through `END_TEST`
   - The registration line: `tcase_add_test(..., test_name);`

## Example

If deleting `test_foo_bar`:

**Delete this block:**
```c
START_TEST(test_foo_bar) {
    // ... all content ...
}
END_TEST
```

**AND delete this line:**
```c
tcase_add_test(tc_core, test_foo_bar);
```

## Rules

- Use the Edit tool for all changes
- Delete the ENTIRE test block, not just the function call
- Delete the MATCHING tcase_add_test line
- Do NOT touch tests not in the list above
- Do NOT modify src/ files
- Do NOT add new code
