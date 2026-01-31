## Objective

All code must pass ThreadSanitizer (data race detection) checks.

## Strategy

1. Run `check-tsan` to identify data race errors
2. For each file with errors:
   - Use `check-tsan --file=<path>` to see all race conditions in that file
   - Read the race report carefully - shows conflicting thread accesses
   - Fix the synchronization issue (add locks, use atomics, eliminate shared state)
   - **Verify `check-tsan` returns `{"ok": true}` after changes**
3. Continue until all data races are resolved

## Common Issues

- **Data races**: Multiple threads accessing same memory without synchronization
- **Lock ordering**: Inconsistent lock acquisition order causing potential deadlocks
- **Missing synchronization**: Shared variables accessed without proper locking

## Guidelines

- ThreadSanitizer shows two conflicting accesses - both need to be synchronized
- Prefer eliminating shared state over adding locks when possible
- If locks are needed, ensure consistent lock ordering to prevent deadlocks
- Do NOT disable ThreadSanitizer warnings - fix the actual race
- Keep changes minimal and focused on the synchronization issue

## Hints

- For naming conventions: `/load style`
- For sanitizer patterns: `/load sanitizers`

## Acceptance

DONE when `check-tsan` returns `{"ok": true}`
