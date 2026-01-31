## Objective

Achieve 90% test coverage (lines, functions, branches) for all source code.

## Output Format

`check-coverage` returns items as objects:
```json
{"ok": false, "items": [{"file": "src/file.c", "lines": "55.7%", "functions": "100%", "branches": "31.9%"}]}
```

## Strategy

1. Run `check-coverage` - returns `{"ok": false, "items": [...]}` with failing files
2. For each item in the response:
   - Item has `file`, `lines`, `functions`, `branches` fields
   - Use `check-coverage --file=<item.file>` to see uncovered lines
   - **Write tests for uncovered code paths** - do NOT modify production code to inflate coverage
   - **Verify `check-coverage` returns `{"ok": true}` after changes**
3. Continue until all coverage thresholds are met

## Guidelines

- **Add tests, not production code** - if code was intentionally removed by prune, don't add it back
- Focus on meaningful test cases that exercise real scenarios
- Cover error paths and edge cases, not just happy paths
- Ensure tests are maintainable and document intent

## Common Gaps

- **Error handling branches**: Error paths not tested
- **Edge cases**: Boundary conditions, null inputs, empty collections
- **Utility functions**: Helper functions not directly tested
- **Conditional logic**: If/else branches not fully covered

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For test structure and patterns: `/load tdd`
- For database fixtures: `/load database`
- For coverage analysis: `/load coverage` or `/load lcov`

## Acceptance

DONE when `check-coverage` returns `{"ok": true}`
