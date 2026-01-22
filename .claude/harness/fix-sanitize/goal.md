## Objective

All code must pass AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer checks.

## Strategy

1. Run `check-sanitize` to identify sanitizer errors
2. For each file with errors:
   - Use `check-sanitize --file=<path>` to see all errors in that file
   - Read the error messages and stack traces carefully
   - Fix the root cause (use-after-free, buffer overflow, undefined behavior, memory leaks)
   - **Verify `check-sanitize` returns `{"ok": true}` after changes**
3. Continue until all sanitizer errors are resolved

## Common Issues

- **heap-buffer-overflow**: Reading/writing past allocated memory bounds
- **use-after-free**: Accessing memory after it was freed (check freed location!)
- **double-free**: Freeing the same memory twice
- **memory leak**: Allocated memory not freed or not attached to talloc hierarchy
- **undefined-behavior**: Integer overflow, null dereference, invalid shifts

## Guidelines

- For use-after-free bugs, start at the FREED location, not the crash location
- Fix ownership/lifetime issues - usually wrong context passed to allocator
- Do NOT add defensive NULL checks everywhere - fix the root cause
- Ensure talloc memory is properly parented to avoid leaks
- Keep changes minimal and focused on the actual bug

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For naming conventions: `/load style`
- For sanitizer patterns: `/load sanitizers`

## Acceptance

DONE when `check-sanitize` returns `{"ok": true}`
