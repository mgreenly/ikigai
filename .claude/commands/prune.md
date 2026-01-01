---
description: Remove dead code (functions unreachable from main) using sub-agents.
---

Remove functions identified as dead code by `scripts/dead-code.sh`.

**Usage:**
- `/prune` - Identify and remove unreachable functions one at a time

---

## Orchestrator Role

You are a **dumb orchestration loop**. Your only job is:
1. Run precondition checks
2. Generate candidate list from script output
3. Spawn sub-agents and wait
4. Count SUCCESS/SKIPPED from sub-agent responses
5. Print summary

**DO NOT:**
- Read source files
- Analyze code
- Make decisions about functions
- Do anything while waiting for sub-agents

All intelligence is in the sub-agents. You are just a dispatcher.

---

## Execution

### Step 1: Preconditions

```bash
git status --porcelain
```

If output is non-empty: print "Error: Git workspace must be clean before pruning." and stop.

### Step 2: Get Candidates

```bash
./scripts/dead-code.sh
```

- If output starts with "No orphaned functions": print "No dead code found." and stop.
- Otherwise: capture lines matching `function:file:line` format into a list

### Step 3: Initialize

Create a single todo item:
- Content: `Pruning N dead code candidates`
- Status: `in_progress`

Initialize counters: `removed=0`, `skipped=0`

### Step 4: Dispatch Loop

For each candidate line `<function>:<file>:<line>`:

1. Spawn sub-agent (configuration below)
2. **WAIT. Do nothing else.**
3. When sub-agent returns, check first word of response:
   - Starts with `SUCCESS` → `removed++`
   - Starts with `SKIPPED` → `skipped++`
4. Continue to next candidate

**Sub-agent configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet`

**Sub-agent prompt:**
```
Remove dead code and verify.

Function: <function>
File: <file>
Line: <line>

Steps:
1. Read `.claude/library/git/SKILL.md` for commit conventions
2. Read the file and locate the function at the specified line
3. Remove the function (and any preceding doc comment block)
4. Run `make bin/ikigai`
   - If FAILS: run `mkdir -p .claude/data && echo "<function>" >> .claude/data/dead-code-false-positives.txt && git checkout -- <file>`
   - Report: SKIPPED: <function> - false positive (recorded)
5. Run `make check`
   - If PASSES: commit with message "refactor: remove dead code <function>" and report: SUCCESS: <function>
   - If FAILS: try to fix/remove failing tests, then `make check` again
6. If tests fixed and pass: commit and report SUCCESS: <function> (N tests modified)
7. If still failing: `git checkout -- .` and report: SKIPPED: <function> - test fixes failed

Response format (first word must be SUCCESS or SKIPPED):
SUCCESS: <function> [optional details]
SKIPPED: <function> - <reason>
```

### Step 5: Summary

After all candidates processed:

```
/prune complete

Removed: <removed>
Skipped: <skipped>
```

Mark todo as `completed`.

---

## Notes

- Orchestrator uses minimal context - just loop control and counters
- All file reading, editing, and decisions happen in sub-agents
- False positives recorded in `.claude/data/dead-code-false-positives.txt`
- To re-check all functions, delete the false positives file
