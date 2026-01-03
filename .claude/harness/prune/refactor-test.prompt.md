# Refactor Test to Use Helper

**UNATTENDED EXECUTION:** Do not ask questions. Just refactor the test.

## Context

The function `{{function}}` was removed from production code. A test helper has been created at `{{helper_header}}`. Update the test to use the helper instead.

## Task

Refactor `{{test_file}}` to include and use the test helper.

## Steps

1. Read `{{test_file}}`
2. Add `#include "../../{{helper_header}}"` (adjust path based on test file location)
3. The function `{{function}}` should now work via the helper

## Rules

- Use the Edit tool for all changes
- Only add the include - the function signature is unchanged
- Adjust the include path based on the test file's directory depth
- Do NOT modify src/ files
- Do NOT modify the helper file
