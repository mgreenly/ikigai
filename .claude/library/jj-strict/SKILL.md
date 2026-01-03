---
name: jj-strict
description: Jujutsu Strict skill for the ikigai project
---

# Jujutsu Strict

## Description
Strict jj operations for release management. Requires all quality gates to pass.

## Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main (bookmark)
- **Upstream**: origin/main

## Commit Policy

### Attribution Rules (MANDATORY)

Do NOT include attributions in commit messages:
- No "Co-Authored-By: Claude <noreply@anthropic.com>"
- No "Generated with [Claude Code](https://claude.com/claude-code)"

This is enforced via CI or pre-push checks. Commits with these lines will be rejected on push.

### Pre-Commit Requirements (MANDATORY)

BEFORE creating ANY commit with source code changes (no exceptions):

1. `make fmt` - Format code
2. `make check` - ALL tests pass (100%)
3. `make lint` - ALL complexity/file size checks pass
4. `make check-coverage` - ALL metrics (lines, functions, branches) at 100.0%
5. `make check-dynamic` - ALL sanitizer checks pass (ASan, UBSan, TSan)

If ANY check fails: fix ALL issues, re-run ALL checks, repeat until everything passes.

Never commit with ANY known issue - even "pre-existing" or "in another file".

**Exception**: Pre-commit checks can be SKIPPED if the commit contains ONLY documentation changes:
- Changes to *.md files (README, docs/, .agents/, etc.)
- Changes to config files (.gitignore, .editorconfig, etc.)
- NO changes to source code (*.c, *.h), Makefile, or build configuration

If ANY source code file is modified, ALL pre-commit checks are required.

## Permitted Operations

This skill permits ALL jj operations including:

- Modifying the `main` bookmark (after all checks pass)
- Creating release bookmarks/tags
- Creating releases
- Force pushing (jj handles automatically)
- All standard jj operations

## Pre-Merge Checklist

Before updating `main`:

1. All pre-commit checks pass (see above)
2. Bookmark is rebased on latest main
3. All CI checks pass (if configured)
4. Code review complete (if required by project policy)

## Release Workflow

```bash
# Ensure on latest main
jj git fetch
jj rebase -d main

# Run all quality checks
make fmt && make check && make lint && make check-coverage && make check-dynamic

# Create release bookmark
jj bookmark create rel-XX

# Push to remote
jj git push --bookmark rel-XX
jj git push --bookmark main
```

## Tagging Convention

Use semantic versioning via bookmarks: `vMAJOR.MINOR.PATCH`

```bash
jj bookmark create v1.2.3
jj describe -m "Release v1.2.3: Brief description"
jj git push --bookmark v1.2.3
```
