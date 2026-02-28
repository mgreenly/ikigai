---
name: jj
description: Jujutsu (jj) skill for the ikigai project
---

# Jujutsu (jj)

## Workflow

**Float on main.** The working copy sits directly on `main@origin`. Manual changes are committed and pushed to main. Most work goes through goals, so you periodically pull from main to pick up completed goal work.

1. **Start fresh**: `jj git fetch` then `jj new main@origin`
2. **Do work**: Make changes, commit as needed
3. **Push to main**: Push directly to main (no bookmarks, no PRs)
4. **Pull updates**: `jj git fetch` then `jj rebase -d main@origin` to pick up goal work

## CRITICAL Rules

- **"All files" means ALL files** - When told to commit, restore, or rebase "all files", include every modified file in the working copy. Never selectively exclude files. The working copy is the source of truth.
- **No bookmarks** - Push directly to main, no feature branches
- **No PRs** - All work lands on main directly

## Starting Work

```bash
jj git fetch
jj new main@origin
```

This puts you on a fresh commit with main as parent. All your work builds from here.

## Committing

When user says "commit", use `jj commit -m "msg"`:

```bash
jj commit -m "Add feature X"
```

Commits stack automatically. After 3 commits you have: `main → A → B → C (@)`

## Pushing to Main

Push all commits directly to main:

```bash
jj git push --revisions 'main@origin..@-'
```

This pushes every commit between main and your current working copy parent.

## Pulling Updates

When goals complete and land on main, pull their work:

```bash
jj git fetch
jj rebase -d main@origin
```

## Squashing (Permission Required)

**NEVER squash without explicit user permission.**

```bash
jj edit <revision>
jj squash -m "Combined message"
```

## Recovery

All operations are logged:
```bash
jj op log
jj op restore <operation-id>
```

## Common Commands

| Task | Command |
|------|---------|
| Fetch remote | `jj git fetch` |
| Start fresh on main | `jj new main@origin` |
| Check status | `jj status` |
| View changes | `jj diff` |
| View log | `jj log` |
| Commit | `jj commit -m "msg"` |
| Push to main | `jj git push --revisions 'main@origin..@-'` |
| Rebase onto main | `jj rebase -d main@origin` |

## Key Concepts

- **Working copy** (`@`): Always a commit being edited
- **main@origin**: The remote main branch — your base and push target
- **Float on main**: Work sits on top of main, pushes go directly to main
