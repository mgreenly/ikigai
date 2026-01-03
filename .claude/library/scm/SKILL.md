---
name: scm
description: Source Code Management skill for the ikigai project
---

# Source Code Management

## Description
Git workflow for preserving all work. Never lose uncommitted code.

## Core Principle

**Every change is committed. Nothing is ever lost.**

You work in a feature worktree that will be squash-merged before release. Individual commit frequency doesn't matter - what matters is that every state is recoverable.

## Rules

### 1. Commit After Every Testable Change

After each TDD cycle (Red → Green → Verify), commit immediately:

```bash
git add -A && git commit -m "descriptive message"
```

Don't batch commits. Don't wait for "a good stopping point." The worktree will be squash-merged anyway - frequent commits only help you.

### 2. Never Have Uncommitted Code at Risk

Before any operation that could lose work:
- Before running `git checkout`
- Before running `git reset`
- Before running `git stash` (prefer commit instead)
- Before closing the session
- Before switching branches

Commit first. Always.

### 3. Experiments: Commit, Try, Revert

When experimenting:

```bash
# 1. Commit the experiment
git add -A && git commit -m "experiment: trying X approach"

# 2. Test/evaluate the experiment

# 3a. If keeping: continue working
# 3b. If discarding: revert cleanly
git revert HEAD --no-edit
```

The experiment is preserved in history even after reverting. You can always recover it.

### 4. Unknown Changes: Commit First, Understand Later

If you encounter changes you don't understand:

```bash
# Wrong: discard unknown changes
git checkout -- .  # DANGEROUS - loses work

# Right: preserve then investigate
git add -A && git commit -m "checkpoint: unknown changes"
git log -p HEAD~1..HEAD  # examine what changed
# Later: revert if unwanted
```

### 5. Discarding Code: Commit, Then Delete

Even when intentionally removing code:

```bash
# 1. Commit current state
git add -A && git commit -m "checkpoint before removing X"

# 2. Delete the code
# 3. Commit the deletion
git add -A && git commit -m "remove X"
```

Now you can recover the deleted code from history if needed.

## Why This Matters

- **Recovery**: Any past state is one `git checkout` away
- **Debugging**: `git bisect` finds which commit broke something
- **Confidence**: Experiment freely knowing nothing is lost
- **Squash merge**: All these commits collapse to one clean commit at release

## Anti-patterns

| Don't | Do Instead |
|-------|------------|
| `git checkout -- .` | Commit first, then revert |
| `git stash` | Commit to a branch |
| `git reset --hard` | Commit first, then reset |
| Batch multiple changes in one commit | Commit after each testable change |
| Leave session with uncommitted work | Commit before stopping |
