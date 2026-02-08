Story: #276

## Objective

Create a new harness script `story-try-close` that checks if a story should be closed after a goal completes.

## Behavior

1. Accept a goal number as argument
2. Fetch the goal issue body via `goal-get`
3. Parse `Story: #N` from the body to find the parent story number
4. List all goals linked to that story (search for issues with `goal` label whose body contains `Story: #N`)
5. If every linked goal has the `goal:done` label, close the story issue via `gh issue close`
6. If not all done, do nothing
7. Return JSON: `{"ok": true, "closed": true/false, "story": N}`

## File Structure

- Script: `.claude/harness/story-try-close/run` (Ruby, matching existing harness conventions)
- Symlink: `.claude/scripts/story-try-close` → `../harness/story-try-close/run`

## Edge Cases

- Goal has no `Story: #N` reference → return ok with closed: false
- Story has no goals → don't close (shouldn't happen but be safe)
- Story is already closed → return ok with closed: false

Story: #276