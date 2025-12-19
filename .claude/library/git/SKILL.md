---
name: git
description: Git skill for the ikigai project
---

# Git

## Description
Standard git operations for day-to-day development work.

## Git Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main

## Commit Policy

### Attribution Rules (MANDATORY)

Do NOT include attributions in commit messages:
- No "Co-Authored-By: Claude <noreply@anthropic.com>"
- No "🤖 Generated with [Claude Code](https://claude.com/claude-code)"

A pre-commit hook enforces this. Commits will be rejected if these lines are present.

### Pre-Commit Checks

Pre-commit checks are NOT required for this skill. Commit freely during development.

However, you should still run `make check` periodically to catch issues early.

## Prohibited Operations

This skill does NOT permit:
- Merging to `main` branch
- Creating tags
- Creating releases
- Force pushing to any branch

These operations require the `git-strict` skill which enforces quality gates.

If you need to perform any of these operations, stop and inform the user they need to load the `git-strict` skill.

## Permitted Operations

- Commit to feature/fix branches
- Push to feature/fix branches
- Create new branches
- Pull/fetch from remote
- Rebase (non-destructive)
- Cherry-pick
