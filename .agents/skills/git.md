# Git

## Description
Load git configuration and commit policies for ikigai.

## Git Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main
- **Upstream**: github/main

## Commit Policy

Do NOT include attributions:
- No "Co-Authored-By: Claude <noreply@anthropic.com>"
- No "🤖 Generated with [Claude Code](https://claude.com/claude-code)"

## Pre-Commit Requirements

BEFORE creating ANY commit with source code changes (mandatory, no exceptions):

1. `make fmt` - Format code
2. `make check` - ALL tests pass (100%)
3. `make lint` - ALL complexity/file size checks pass
4. `make coverage` - ALL metrics (lines, functions, branches) at 100.0%
5. `make check-dynamic` - ALL sanitizer checks pass (ASan, UBSan, TSan)

If ANY check fails: fix ALL issues, re-run ALL checks, repeat until everything passes.

Never commit with ANY known issue - even "pre-existing" or "in another file".

**Exception**: Pre-commit checks can be SKIPPED if the commit contains ONLY documentation changes:
- Changes to *.md files (README, docs/, .agents/, etc.)
- Changes to .gitignore, .editorconfig, or similar config files
- NO changes to source code (*.c, *.h), Makefile, or build configuration

If ANY source code file is modified, ALL pre-commit checks are required.
