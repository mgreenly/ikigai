Run coverage tests and fix gaps using sub-agents.

**Usage:**
- `/coverage` - Run make coverage and dispatch sub-agents to fix coverage gaps

**Action:** Runs full coverage, identifies gaps from structured reports, then dispatches sequential sub-agents (one per file) to fix them. Sub-agents commit incrementally to avoid losing work to context exhaustion.

---

## Execution

### 1. Run Full Coverage

Run `make coverage` to generate the coverage report.

Parse `reports/coverage/summary.txt` to find files with <100% coverage. The format is:

```
                                    |Lines      |Functions |Branches
Filename                            |Rate    Num|Rate  Num|Rate   Num
=====================================================================
[/home/.../src/]
config.c                            |54.0%   150|50.0%   4|40.4%  114
commands_fork.c                     |87.4%    95| 100%   2|81.2%   16
```

Extract files where any rate column is not "100%" or "-" (dash means no data).

If all files show 100% (or dash), report success and stop.

### 2. Sort and Create Todo List

**Sort files by gap size before creating todos:**

For each file with gaps, calculate a gap score:
- **Uncovered branches** = Total branches × (100 - Branch%) / 100
- **Uncovered lines** = Total lines × (100 - Line%) / 100
- **Uncovered functions** = Total functions × (100 - Function%) / 100
- **Gap score** = (Uncovered branches × 3) + (Uncovered lines × 2) + (Uncovered functions × 1)

Sort files by gap score **descending** (highest gap first). This ensures we tackle the largest coverage gaps first, maximizing overall improvement.

Use TodoWrite to create a task for each file in sorted order. Include:
- Source file path (relative to src/)
- Current coverage rates (lines/functions/branches)
- Gap score (for reference)

### 3. Fix Issues Sequentially

For each file in the todo list, spawn a fix sub-agent:

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet`

**CRITICAL:** Run sub-agents ONE AT A TIME. Wait for each to complete before spawning the next. The build system does not support concurrent coverage builds - parallel execution will corrupt results.

**Prompt template:**
```
## Context

Load these skills:
- `/skillset coverage` - Coverage methodology and patterns
- `/load git` - Git conventions for commits

## Task

Improve coverage in: src/{FILE_PATH}

Current coverage:
- Lines: {LINE_RATE}
- Functions: {FUNC_RATE}
- Branches: {BRANCH_RATE}

## Success Criteria

**Your goal is PROGRESS, not perfection.** A committed 10% improvement is infinitely more valuable than an uncommitted 50% improvement that exhausts your context.

Prioritize:
1. **Easy wins first** - Simple branches, obvious test cases, low-hanging fruit
2. **Commit early, commit often** - After each meaningful improvement, commit it
3. **Diminishing returns awareness** - When gains slow down, wrap up what you have

## Context Budget

You have ~200K tokens total. **At 140K usage, stop new work and wrap up:**
1. Commit any uncommitted improvements
2. Run `make lint` and `make check` to validate
3. Report your gains and stop

**DO NOT** burn remaining context chasing difficult edge cases. Capture your wins.

## Resources

### Coverage Map
Read `.claude/data/source_tests.json` to find which tests exercise this file.
Format: `"src/file.c": ["test/path1", "test/path2", ...]`

If the file isn't in the map, or you need a new test, create one following existing patterns in `tests/unit/`.

### Targeted Coverage
Run `make coverage TEST=<test_name>` to get focused feedback. Examples:
- `make coverage TEST=config/config_test`
- `make coverage TEST=db/agent_errors_test`

This generates: `reports/coverage/unit/<test_path>.coverage.txt`

**IMPORTANT:** The per-test report shows ALL source files, but most will be 0% (not exercised by that test). Focus ONLY on the row for `{FILE_PATH}` - ignore all other files.

## Process

1. Read the source file and identify uncovered code
2. **Triage branches by difficulty** - mark easy vs hard
3. Check coverage map for existing tests
4. **Start with easiest wins** - implement and verify
5. Run targeted coverage after each change
6. **Commit after each successful improvement** (follow git skill conventions)
7. Repeat steps 4-6 for next-easiest wins
8. When gains slow OR context reaches 140K: run `make lint && make check`, commit remaining work, report and stop

Report: starting coverage → final coverage → what you fixed → what remains (if any)
```

### 4. Verify Results

After ALL sub-agents complete, run `make coverage` directly and check `reports/coverage/summary.txt`.

- If all files now show 100%: report success
- If gaps remain: compare to initial state

### 5. Progress Loop

If gaps remain after a full pass:
- **Forward progress** = fewer files with gaps OR improved rates
- If progress: return to Step 1 with fresh discovery
- If no progress: stop and report remaining gaps to user

Then wait for instructions.
