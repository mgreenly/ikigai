## Objective

All code must pass Valgrind Helgrind (thread error detection) checks.

## Strategy

1. Run `check-helgrind` to identify thread synchronization errors
2. For each file with errors:
   - Use `check-helgrind --file=<path>` to see all errors in that file
   - Read the error messages carefully - shows lock ordering and data races
   - Fix the synchronization issue (consistent lock ordering, proper locking)
   - **Verify `check-helgrind` returns `{"ok": true}` after changes**
3. Continue until all Helgrind errors are resolved

## Common Issues

- **Lock order violations**: Inconsistent lock acquisition order (potential deadlock)
- **Data races**: Conflicting accesses to shared memory without locks
- **Unlocked access**: Accessing protected data without holding the lock

## Guidelines

- For lock ordering violations, establish a consistent global lock order
- Ensure all accesses to shared data are protected by the same lock
- Do NOT disable Helgrind warnings - fix the actual thread safety issue
- Keep changes minimal and focused on the synchronization problem

## Hints

- For naming conventions: `/load style`
- For sanitizer patterns: `/load sanitizers`

## Acceptance

DONE when `check-helgrind` returns `{"ok": true}`
