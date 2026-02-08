Story: #268

## Objective

Modify `.claude/harness/ralph/run` to capture commit hashes and diff stats, and include them in the `stats.jsonl` record.

## Steps

1. Before the main loop, capture the current commit hash:
   ```ruby
   start_commit = `jj log -r @ --no-graph -T 'commit_id' 2>/dev/null`.strip
   ```

2. After the main loop exits, capture the end commit hash:
   ```ruby
   end_commit = `jj log -r @ --no-graph -T 'commit_id' 2>/dev/null`.strip
   ```

3. Compute diff stats between the two commits:
   ```ruby
   stat_output = `jj diff --stat --from #{start_commit} --to #{end_commit} 2>/dev/null`
   ```
   Parse the summary line (e.g. `3 files changed, 40 insertions(+), 5 deletions(-)`) to extract `lines_added` and `lines_deleted`. If parsing fails or commits are identical, use 0.

4. Add four fields to the JSON record in `write_stats_record`:
   - `commit_start` (string or null)
   - `commit_end` (string or null)
   - `lines_added` (integer)
   - `lines_deleted` (integer)

## Acceptance Criteria

- Stats record includes all four new fields
- Values are correct when Ralph makes changes
- Values are 0/identical hashes when Ralph makes no changes
- Existing functionality is not affected

Story: #268