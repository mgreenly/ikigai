# Git Workflow with Release Branches and Worktrees

This project uses release branches with git worktrees for all development. See [decisions/git-workflow-release-branches.md](decisions/git-workflow-release-branches.md) for rationale.

## Repository Structure

```
projects/ikigai/              # bare repo
  ├── main/                   # main worktree (always pristine)
  ├── rel-NN/                 # release worktree (contains release/ folder)
  ├── feature-XYZ/            # feature worktrees
  └── hotfix-NNN/             # hotfix worktrees
```

## Core Principle

**Never work directly on main.** All development happens in release branches. Main only receives FF-merges from completed, quality-gated releases.

The `release/` folder (plan, tasks, user-stories, research) is gitignored and only exists in release worktrees during active development.

---

## Starting a New Release

```bash
cd projects/ikigai
git worktree add rel-NN -b rel-NN main
cd rel-NN

# Build release/ folder with RPI pipeline
# (research → plan → verify → tasks)
```

**The release/ folder lives here and nowhere else.**

---

## Implementing a Feature

### Create Feature Branch

```bash
cd projects/ikigai
git worktree add feature-XYZ -b feature-XYZ rel-NN
cd feature-XYZ
```

### Implement and Test

Work in the feature worktree:
- Write code
- Run tests locally
- Fix issues

### Quality Gates

Before merging, all checks must pass:

```bash
make check           # Unit tests
make coverage        # 100% line and branch coverage
make check-sanitize  # ASan + UBSan
make check-tsan      # ThreadSanitizer
make check-valgrind  # Memcheck
make check-helgrind  # Thread errors
```

Or run all in sequence:

```bash
.claude/harness/quality/run
```

### Merge to Release Branch

Once quality gates pass:

```bash
cd ../rel-NN
git merge --squash feature-XYZ
git commit -m "feat: description of feature"

# Clean up
git worktree remove ../feature-XYZ
git branch -d feature-XYZ
```

Repeat for all features in the release.

---

## Handling Hotfixes

Hotfixes go directly to main, then release branch rebases.

### Create Hotfix Branch

```bash
cd projects/ikigai
git worktree add hotfix-NNN -b hotfix-NNN main
cd hotfix-NNN
```

### Fix and Test

Quality gates apply (same as features).

### Merge to Main

```bash
cd ../main
git merge --squash hotfix-NNN
git commit -m "fix: description of fix"

# Clean up
git worktree remove ../hotfix-NNN
git branch -d hotfix-NNN
```

### Rebase Release Branch

**Important:** Finish all active feature branches before rebasing.

```bash
# Ensure no features in progress
cd projects/ikigai
ls -d feature-* 2>/dev/null && echo "Finish features first!" || echo "Ready to rebase"

# Rebase release onto updated main
cd rel-NN
git rebase main
```

Resume feature development after rebase.

---

## Completing a Release

When all features are implemented and merged to release branch:

### Final Quality Check

```bash
cd projects/ikigai/rel-NN
.claude/harness/quality/run
```

All checks must pass.

### Merge to Main

```bash
cd ../main
git merge --ff-only rel-NN
```

If FF-merge fails, the release branch diverged from main. This shouldn't happen if hotfixes were rebased correctly.

### Tag Release

```bash
git tag v0.N.0
git push origin main --tags
```

### Clean Up

```bash
# Delete release/ folder (gitignored, won't be committed)
rm -rf release/

# Optional: Archive release artifacts outside git
mkdir -p ../.archives/rel-NN
cp -r ../rel-NN/release/ ../.archives/rel-NN/

# Remove worktree and branch
cd ..
git worktree remove rel-NN
git branch -d rel-NN
```

---

## Common Scenarios

### Checking Worktree Status

```bash
cd projects/ikigai
git worktree list
```

### Switching Between Worktrees

Don't use `git checkout`. Just `cd` to the worktree directory:

```bash
cd projects/ikigai/rel-08      # Work on release
cd projects/ikigai/feature-XYZ # Work on feature
cd projects/ikigai/main        # Check main state
```

### Viewing Release Plan

The `release/` folder only exists in release worktrees:

```bash
cd projects/ikigai/rel-NN
ls release/
# plan/ tasks/ user-stories/ research/ verified.md ...
```

### Parallel Feature Development

Multiple features can be worked on simultaneously in different worktrees:

```bash
cd projects/ikigai
git worktree add feature-auth -b feature-auth rel-08
git worktree add feature-logging -b feature-logging rel-08

# Work on both in parallel (different terminal windows)
cd feature-auth    # Terminal 1
cd feature-logging # Terminal 2
```

**Merge them sequentially** when ready (one at a time, not in parallel).

### Abandoned Features

If a feature is abandoned:

```bash
cd projects/ikigai
git worktree remove feature-XYZ
git branch -D feature-XYZ  # Force delete unmerged branch
```

The release branch is unaffected.

---

## Workflow Rules

1. **Never commit release/ folder** - It's gitignored
2. **Never work directly on main** - Always use release branches
3. **Quality gates before every merge** - No exceptions
4. **Squash merge features to release** - Clean feature-level commits
5. **FF-merge release to main** - Preserves feature history
6. **Finish features before rebasing** - Simplifies conflict resolution
7. **One feature merge at a time** - Sequential, not parallel

---

## Why This Works

**For humans:**
- Clear separation of concerns
- Parallel development without branch switching
- Main always pristine and bisect-friendly

**For AI agents:**
- Deprecated code completely removed before replacement
- No confusion between old and new implementations
- Uniform pattern (no conditional logic)

**For the project:**
- Every commit on main is functional
- Git history is clean and navigable
- Release boundaries are explicit (branch lifetime)
