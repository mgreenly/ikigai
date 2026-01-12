## Objective

All tests must pass: `.claude/harness/check/run` returns `{"ok": true}`.

## Context

- C with talloc memory management (hierarchical, parent-child ownership)
- Result types with OK()/ERR() patterns for error handling
- Unit tests via the Check framework (XML reports in reports/check/)
- PostgreSQL for database tests (test databases named ikigai_test_*)

## Tools

Use the check harness for discovery and verification - it's token-efficient:

```bash
.claude/harness/check/run
```

Returns:
- `{"ok": true}` - all tests pass
- `{"ok": false, "items": ["file:line func: message", ...]}` - failures

For single-file testing during fixes (faster feedback):
```bash
make check TEST=tests/unit/path/file_test.c
```

## Skills

**Load as needed for domain knowledge:**

| Skill | When |
|-------|------|
| `/load errors` | Result types, OK()/ERR() patterns, error propagation |
| `/load memory` | talloc ownership, contexts, stealing, reparenting |
| `/load database` | PostgreSQL queries, test fixtures, connection handling |
| `/load style` | Naming conventions, code patterns, project idioms |
| `/load tdd` | Test structure, Check framework macros, assertions |

## Approach

- Run check harness to get current failures
- Look for patterns in failures (same file, same module, related errors)
- Fix foundational issues first - cascading failures often share a root cause
- Use single-file test runs for faster feedback while fixing

## Troubleshooting

- **Assertion failures**: Check test expectations vs actual behavior
- **Segfaults**: Usually memory issues - check talloc ownership, NULL pointers
- **Memory leaks**: Ensure proper talloc hierarchy, use `talloc_free()` on root
- **Database errors**: Check connection handling, transaction cleanup
- **Stuck on a domain**: Load the relevant skill for deeper context

## Acceptance

`.claude/harness/check/run` returns `{"ok": true}`
