---
description: Remove dead code (functions unreachable from main) using sub-agents.
---

Remove functions identified as dead code by `scripts/dead-code.sh`.

**Usage:**
- `/prune` - Identify and remove unreachable functions one at a time

---

## Pre-conditions

1. Run `git status --porcelain`
2. If output is non-empty, abort: "Error: Git workspace must be clean before pruning."

## Execution

### 1. Identify Dead Code

Run `scripts/dead-code.sh` and parse output.

- If "No orphaned functions found." → report success and stop
- Otherwise, parse lines matching `function:file:line` format

### 2. Create Todo List

Use TodoWrite to create a todo item for each dead function:
- Content: `Remove dead function: <function_name> (<file>:<line>)`
- Status: `pending`

### 3. Process Each Function

**Sequence:** Process ONE function at a time. Do not start the next until the current completes.

**CRITICAL:** While a sub-agent is running, do NOTHING. Do not read files, edit files, run commands, or perform any other actions.

For each function, mark it `in_progress` in the todo list, then spawn a sub-agent:

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet` with thinking enabled

**Prompt template:**
```
Remove dead code function and verify the change.

**Pre-read skills:**
- `/load git` - Commit conventions (required)

**Load on-demand (only if refactoring tests):**
- `/load style` - Code style rules
- `/load tdd` - Test patterns
- `/load mocking` - If tests use mock overrides

**Function:** <function_name>
**Location:** <file>:<line>

**Steps:**

1. Read the file and locate the function
2. Remove the entire function (including any preceding doc comment)
3. Run `make bin/ikigai`
   - If build fails: run `git checkout -- <file>`, report SKIPPED (false positive), done
4. Run `make check`
   - If passes: commit and report SUCCESS, done
   - If fails: analyze which test(s) failed

5. For each failing test:
   - Read the test file
   - Determine if the test ONLY tests the removed function
     - If yes: remove the entire test
     - If no: refactor the test to remove usage of the dead function

6. Run `make check` again
   - If passes: commit all changes and report SUCCESS, done
   - If fails: run `git checkout -- .`, report SKIPPED (unresolvable), done

**Commit message format:**
```
refactor: remove dead code <function_name>

Function unreachable from main(), identified by scripts/dead-code.sh.
[Optional: Also removed/refactored test(s) that depended on it.]
```

**Report format:**
Return one of:
- SUCCESS: removed <function_name>, [N test(s) removed/refactored]
- SKIPPED: <function_name> - <reason>
```

### 4. Update Todo and Track Results

After each sub-agent completes:
- Parse the report (SUCCESS or SKIPPED)
- Mark todo item as `completed`
- Track counts: removed, skipped, tests_modified

### 5. Summary

After all functions processed, output:

```
/prune complete

Removed: N functions
Skipped: M functions (false positives or unresolvable)
Tests modified: K

[If any skipped, list them with reasons]
```

## Notes

- Each removal is an atomic commit
- Skipped functions remain in codebase (may need manual review)
- The dead-code script may have false positives for function-pointer dispatch
