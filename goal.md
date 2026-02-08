Story: #0

## Objective

After the orchestrator creates a PR via `gh pr create`, it should immediately enable auto-merge on that PR so it merges automatically once CI passes, without relying on the "Auto-merge Bot PRs" GitHub Actions workflow.

## Current Behavior

In `.claude/harness/orchestrator/run`, the `create_pr_from_clone` method creates a PR and returns the PR number. Auto-merge is not enabled — it relies on a separate GitHub Actions workflow which is unreliable.

## Required Change

In the `create_pr_from_clone` method, after the PR is successfully created (after extracting `pr_num` from the `gh pr create` output), add a call to enable auto-merge:

```ruby
system('gh', 'pr', 'merge', pr_num, '--auto', '--squash',
       chdir: clone_dir, out: '/dev/null', err: '/dev/null')
```

This should go right after `pr_num = pr_output.strip.split('/').last` and before the method returns `pr_num`.

## Acceptance Criteria

- After `gh pr create` succeeds, `gh pr merge --auto --squash` is called on the new PR.
- Failure of the auto-merge command is non-fatal (the PR still exists, just won't auto-merge).
- No other behavior changes.