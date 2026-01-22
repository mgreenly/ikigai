## Objective

All code must pass Valgrind Memcheck (memory error detection) checks.

## Strategy

1. Run `check-valgrind` to identify memory errors
2. For each file with errors:
   - Use `check-valgrind --file=<path>` to see all errors in that file
   - Read the error messages and stack traces carefully
   - Fix the root cause (invalid read/write, use of uninitialized values, memory leaks)
   - **Verify `check-valgrind` returns `{"ok": true}` after changes**
3. Continue until all Valgrind errors are resolved

## Common Issues

- **Invalid read/write**: Accessing memory outside allocated bounds
- **Uninitialized value**: Using variables before they are set
- **Memory leak**: Allocated memory not freed (definitely lost, possibly lost)
- **Invalid free**: Freeing memory that wasn't allocated or already freed

## Guidelines

- Pay attention to the origin of uninitialized values in stack traces
- Ensure all paths initialize variables before use
- For leaks, ensure talloc memory is properly parented
- Do NOT suppress Valgrind errors - fix the actual bug
- Keep changes minimal and focused on the memory error

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`
- For Valgrind patterns: `/load valgrind`

## Acceptance

DONE when `check-valgrind` returns `{"ok": true}`
