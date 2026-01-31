## Objective

All code must compile successfully.

## Check Command

Run `.claude/scripts/check-compile` to check status.

## Strategy

1. Run `.claude/scripts/check-compile` to identify compilation errors
2. For each file with errors:
   - Use `.claude/scripts/check-compile --file=<path>` to see all errors in that file
   - Read the error messages carefully - compiler errors often cascade
   - Fix the root cause (usually the first error in a file)
   - **Verify with `.claude/scripts/check-compile --file=<path>` after changes**
3. Continue until all compilation errors are resolved

## Common Issues

- **Implicit declarations**: Missing `#include` statements or forward declarations
- **Type mismatches**: Incorrect function signatures or variable types
- **Syntax errors**: Typos, missing semicolons, unmatched braces
- **Missing headers**: Forward declarations needed for struct pointers

## Guidelines

- Fix one error at a time - subsequent errors often resolve automatically
- Check function signatures match between declaration and definition
- Verify header guards are correct

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`

## Acceptance

DONE when `.claude/scripts/check-compile` returns `{"ok": true}`
