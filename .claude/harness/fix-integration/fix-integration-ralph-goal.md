## Objective

All integration tests must pass.

## Strategy

1. Run `check-integration` to identify failing tests
2. For each file with failing tests:
   - Use `check-integration --file=<path>` to see relevant integration test failures
   - Read the test output and failure messages
   - Understand the end-to-end workflow being tested
   - Fix the implementation to handle the integration scenario
   - **Verify `check-integration` returns `{"ok": true}` after changes**
3. Continue until all integration tests pass

## Guidelines

- Integration tests verify components work together correctly
- Focus on interface boundaries and data flow between components
- Check database state, API responses, and side effects
- Ensure proper error handling across component boundaries

## Common Issues

- **Database state**: Test data not properly set up or cleaned up
- **Configuration**: Missing or incorrect environment variables
- **API contracts**: Components using incompatible interfaces
- **Resource cleanup**: Connections, files, or processes not properly closed

## Hints

- For talloc/ownership patterns: `/load memory`
- For Result type patterns: `/load errors`
- For test structure and fixtures: `/load tdd`
- For database fixtures and queries: `/load database`

## Acceptance

DONE when `check-integration` returns `{"ok": true}`
