Story: #0

## Objective

Prevent merge conflicts caused by ralph runtime files being committed into PRs. All runtime files that ralph and the orchestrator write into clone directories must go into `.pipeline/cache/`, which is gitignored.

## Problem

When ralph runs in a clone, it writes these files to the repo root:
- `goal.md` (written by the orchestrator before spawning ralph)
- `goal-progress.jsonl` (written by ralph during execution)
- `goal-summary.md` (written by ralph during execution)
- `goal-summary-meta.json` (written by ralph during execution)

These are NOT gitignored at the repo root level. jj tracks them, ralph commits them, and they end up in PRs. When multiple goals run concurrently, these files conflict.

## Required Changes

### 1. Add `.pipeline/cache/` to `.gitignore`

Add this line to the project `.gitignore`:
```
.pipeline/cache/
```

### 2. Update orchestrator to write `goal.md` into `.pipeline/cache/`

In `.claude/harness/orchestrator/run`, in the `create_pr_from_clone` method area where `goal.md` is written (around line 305-307):

- Create `.pipeline/cache/` directory in the clone
- Write goal body to `.pipeline/cache/goal.md` instead of `goal.md`
- Update the ralph spawn args to use `--goal=.pipeline/cache/goal.md` instead of `--goal=goal.md`

### 3. Update ralph to write runtime files into `.pipeline/cache/`

In `.claude/harness/ralph/run`, ralph derives runtime file paths from the goal file path (progress, summary, summary-meta). Since the goal file will now be at `.pipeline/cache/goal.md`, the derived paths should automatically land in `.pipeline/cache/` IF ralph derives them relative to the goal file's directory.

Verify that ralph's path derivation logic places these files in `.pipeline/cache/`:
- `.pipeline/cache/goal-progress.jsonl`
- `.pipeline/cache/goal-summary.md`
- `.pipeline/cache/goal-summary-meta.json`

If ralph derives paths differently (e.g., always in the working directory root), update the derivation logic so these files land in `.pipeline/cache/`.

### 4. No other changes

- Do not change retry logic, label transitions, dependency checking, PR creation, or any other behavior.
- Do not change the stats.jsonl or goal archive paths in `~/.local/state/ralph/` — those are fine.

## Acceptance Criteria

- `.pipeline/cache/` is in `.gitignore`.
- The orchestrator writes `goal.md` to `.pipeline/cache/goal.md` in clones.
- Ralph's runtime files (progress, summary, summary-meta) land in `.pipeline/cache/`.
- No runtime files are written to the repo root.
- Existing ralph functionality (iterations, commits, stats) is unchanged.