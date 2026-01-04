---
name: jj
description: Jujutsu (jj) skill for the ikigai project
---

# Jujutsu (jj)

## Description
Standard jj operations for day-to-day development work.

## Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main (bookmark)

## Commit Policy

### Commit Workflow

jj has no staging area. All file changes are automatically part of the working copy commit.

```bash
# Describe current working copy
jj describe -m "Your message"

# Commit and start new empty working copy
jj commit -m "Your message"

# Squash working copy into parent
jj squash
```

Run `make check` periodically to catch issues early.

## Prohibited Operations

This skill does NOT permit:
- Modifying the `main` bookmark
- Creating tags
- Creating releases
- Force pushing to main

These operations require the `jj-strict` skill which enforces quality gates.

If you need to perform any of these operations, stop and inform the user they need to load the `jj-strict` skill.

## Permitted Operations

- Commit to feature/fix bookmarks
- Push feature/fix bookmarks to remote
- Create new bookmarks
- Fetch from remote
- Rebase commits
- Create new commits on any mutable revision

## Common Commands

| Task | Command |
|------|---------|
| Check status | `jj status` |
| View changes | `jj diff` |
| View log | `jj log` |
| Describe commit | `jj describe -m "msg"` |
| Commit (new working copy) | `jj commit -m "msg"` |
| Squash into parent | `jj squash` |
| Create bookmark | `jj bookmark create <name>` |
| Push bookmark | `jj git push --bookmark <name>` |
| Fetch from remote | `jj git fetch` |
| Restore working copy | `jj restore` |
| Edit existing commit | `jj edit <revision>` |
| Create commit on revision | `jj new <revision>` |
| Rebase | `jj rebase -d <destination>` |

## Key Concepts

### Working Copy is Always a Commit
In jj, `@` (working copy) is always a commit being edited. There's no staging area.

### Bookmarks vs Branches
jj "bookmarks" are equivalent to git "branches". They're just named pointers to commits.

### Immutable vs Mutable
- `◆` = immutable (protected, can't change)
- `○` = mutable (can still edit)
- `@` = current working copy
