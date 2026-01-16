---
name: jj
description: Jujutsu (jj) skill for the ikigai project
---

# Jujutsu (jj)

## Description
Standard jj operations for day-to-day development work.

## CRITICAL: Commits Are Permanent

**When the user says "commit", you MUST immediately create an immutable commit using `jj commit -m "msg"`.**

Commits in jj are:
- **Permanent** - Stored in the operation log forever
- **Immutable** - Cannot be lost or overwritten
- **Recoverable** - Can always be restored via `jj op restore`

Working copy changes that are NOT committed:
- **Can be lost** - Rebases, restores, and other operations can discard them
- **Are temporary** - Only the most recent snapshot is preserved
- **Are NOT permanent** - Must be committed to be safe

**There is no "half-assed" commit. When you commit, it's permanent. If you don't commit, changes can be lost.**

When the user asks you to commit:
1. Use `jj commit -m "descriptive message"` immediately
2. This creates a permanent, immutable commit
3. The changes are now safe and recoverable forever

## Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main (bookmark)

## Commit Policy

**When user says "commit": IMMEDIATELY use `jj commit -m "msg"`**

- NOT `jj describe` (only updates description, doesn't create new commit)
- NOT "I'll commit later" (changes can be lost)
- NOT "working on it" (working copy is mutable)
- YES `jj commit -m "descriptive message"` (creates permanent immutable commit)

**Every commit you create is permanent and can never be lost.**

Run `make check` periodically to catch issues early.

## Prohibited Operations

This skill does NOT permit:
- Modifying the `main` bookmark locally
- Merging into main locally
- Force pushing to main

**Merges to main only happen via GitHub PRs.** All work is done on feature bookmarks, pushed to origin, and merged through pull requests on GitHub. Never merge locally.

## Permitted Operations

- Commit to feature/fix bookmarks
- Push feature/fix bookmarks to remote
- Create new bookmarks
- Create and push tags
- Fetch from remote
- Rebase commits
- Create new commits on any mutable revision

## Common Commands

| Task | Command |
|------|---------|
| Check status | `jj status` |
| View changes | `jj diff` |
| View log | `jj log` |
| **Commit all files** | **`jj commit -m "msg"`** |
| Squash into parent | `jj squash` |
| Create bookmark | `jj bookmark create <name>` |
| Update bookmark to @ | `jj bookmark set <name>` |
| Push bookmark | `jj git push --bookmark <name>` |
| Fetch from remote | `jj git fetch` |
| Restore working copy | `jj restore` |
| Edit existing commit | `jj edit <revision>` |
| Create commit on revision | `jj new <revision>` |
| Rebase | `jj rebase -d <destination>` |
| Create tag | `jj tag set <name> -r <revision>` |
| Push tag | `git push origin <tag>` |
| List tags | `jj tag list` |

## Key Concepts

### Working Copy is Always a Commit
In jj, `@` (working copy) is always a commit being edited. There's no staging area.

### Bookmarks vs Branches
jj "bookmarks" are equivalent to git "branches". They're just named pointers to commits.

### Immutable vs Mutable
- `◆` = immutable (protected, permanently committed)
- `○` = mutable (can still edit, NOT permanently committed yet)
- `@` = current working copy (MUTABLE - changes can be lost until committed)

**Key insight**: Only immutable commits (◆) are truly permanent. Mutable commits (○) and working copy (@) changes can be lost. When you run `jj commit -m "msg"`, the current working copy becomes an immutable commit.

### "Update the bookmark"
When the user says "update the bookmark", find the most recent bookmark in `@`'s ancestry and move it to `@` using `jj bookmark set <name>`.
