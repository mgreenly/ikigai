## Objective

All binaries must link successfully.

## Check Command

Run `make check-link` to check status.

## Strategy

1. Run `make check-link` to identify linking errors
2. For each binary with errors:
   - Use `make check-link FILE=<binary>` to see specific errors
   - Identify the missing or duplicate symbols
   - Fix the root cause
   - **Verify with `make check-link FILE=<binary>` after changes**
3. Continue until all binaries link

## Common Issues

- **Undefined references**: Missing function implementations
- **Multiple definitions**: Same symbol defined in multiple objects
- **Missing libraries**: Library not linked (-l flag missing)
- **Missing objects**: Source file not compiled or not included in link

## Guidelines

- Check that all required source files are being compiled
- Verify function declarations match definitions exactly
- For tests: ensure mocks/helpers are discovered via .d files
- For duplicate symbols: use `--allow-multiple-definition` or fix the duplication

## Hints

- For Makefile structure: `/load makefile`
- For test linking patterns: see `.make/check-link.mk`
- Mock discovery uses `_helper.h` and `_mock.h` patterns

## Acceptance

DONE when `make check-link` shows ✅ All binaries linked
