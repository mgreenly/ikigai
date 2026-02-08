Story: #273

## Objective

Fix Ralph's commit process to exclude its own runtime files (`goal-progress.jsonl`, `goal.md`, `ralph.log`) from commits.

## Investigation Needed

1. Find how Ralph stages and commits files in `.claude/harness/ralph/run`
2. Determine whether it uses `jj` track/untrack, `.gitignore`, or explicit file lists
3. Fix the commit step to exclude runtime artifacts

## Acceptance Criteria

- Ralph commits never include `goal-progress.jsonl`, `goal.md`, or `ralph.log`
- Actual code changes are still committed normally
- No impact on Ralph's ability to read/write these files during execution

Story: #273