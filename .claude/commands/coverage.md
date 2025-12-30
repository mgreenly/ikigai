Run coverage tests and fix gaps using sub-agents.

**Usage:**
- `/coverage` - Run make coverage and dispatch sub-agents to fix coverage gaps

**Action:** Runs full coverage, identifies gaps from structured reports, then dispatches sequential sub-agents (one per file) to fix them.

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

### 2. Create Todo List

Use TodoWrite to create a task for each file with coverage gaps. Include:
- Source file path (relative to src/, e.g., `config.c`, `db/agent.c`)
- Current coverage rates (lines/functions/branches)

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
- `/persona coverage` - Coverage methodology and patterns
- `/load git` - Git conventions for commits

## Task

Fix coverage gaps in: src/{FILE_PATH}

Current coverage:
- Lines: {LINE_RATE}
- Functions: {FUNC_RATE}
- Branches: {BRANCH_RATE}

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

1. Check coverage map for existing tests that touch this file
2. Read the source file to understand what's not covered
3. Decide: modify existing test OR create new test
4. Run targeted coverage to verify your changes improve the specific file
5. Iterate until coverage improves
6. Run `make lint` and `make check` to validate
7. Commit the changes (follow git skill conventions)

Report what you fixed and final coverage for this file.
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
