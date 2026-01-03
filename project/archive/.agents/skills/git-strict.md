# Git Strict

## Description
Strict git operations for release management. Requires all quality gates to pass.

## Git Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main
- **Upstream**: github/main

## Commit Policy

### Attribution Rules (MANDATORY)

Do NOT include attributions in commit messages:
- No "Co-Authored-By: Claude <noreply@anthropic.com>"
- No "🤖 Generated with [Claude Code](https://claude.com/claude-code)"

A pre-commit hook enforces this. Commits will be rejected if these lines are present.

### Pre-Commit Requirements (MANDATORY)

BEFORE creating ANY commit with source code changes (no exceptions):

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

## Permitted Operations

This skill permits ALL git operations including:

- Merging to `main` branch (after all checks pass)
- Creating tags
- Creating releases
- Force pushing (use with caution)
- All standard git operations

## Pre-Merge Checklist

Before merging to `main`:

1. All pre-commit checks pass (see above)
2. Branch is up-to-date with main (rebase if needed)
3. All CI checks pass (if configured)
4. Code review complete (if required by project policy)

## Tagging Convention

Use semantic versioning: `vMAJOR.MINOR.PATCH`

Example:
```bash
git tag -a v1.2.3 -m "Release v1.2.3: Brief description"
git push origin v1.2.3
```
