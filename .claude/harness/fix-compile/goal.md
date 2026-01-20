## Objective

All code must compile and link successfully.

## Strategy

1. Run `check-compile` to identify compilation/linking errors
2. For each file with errors:
   - Use `check-compile --file=<path>` to see all errors in that file
   - Read the error messages carefully - compiler errors often cascade
   - Fix the root cause (usually the first error in a file)
   - **Verify `check-compile` returns `{"ok": true}` after changes**
3. Continue until all compilation errors are resolved

## Common Issues

- **Implicit declarations**: Missing `#include` statements or forward declarations
- **Type mismatches**: Incorrect function signatures or variable types
- **Undefined references**: Missing function implementations or linker issues
- **Syntax errors**: Typos, missing semicolons, unmatched braces

## Guidelines

- Fix one error at a time - subsequent errors often resolve automatically
- Check function signatures match between declaration and definition
- Ensure all source files are included in Makefile targets
- Verify header guards are correct

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`
- For Makefile structure: `/load makefile`

## Acceptance

DONE when `check-compile` returns `{"ok": true}`
