Story: #268

## Objective

Add null values for the four new fields to all existing records in `$HOME/.local/state/ralph/stats.jsonl`.

## Steps

1. Read each line of `stats.jsonl`
2. Parse as JSON
3. If the record lacks `commit_start`, `commit_end`, `lines_added`, or `lines_deleted`, set them to `null`
4. Write the updated records back to the file

## Implementation

A simple inline script (Ruby, Python, or shell+jq) run once. Can be done as a one-liner or small script.

## Acceptance Criteria

- All existing records have the four new fields set to `null`
- No data loss — all original fields preserved
- New records (from goal 1) are not affected

Story: #268
