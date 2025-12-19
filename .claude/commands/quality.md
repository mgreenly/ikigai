Run all quality checks in a fix-point loop until stable.

**Usage:**
- `/quality` - Run lint, check, sanitize, tsan, valgrind, helgrind, coverage in sequence

**Goal:** Achieve a clean run through all checks with zero code changes. If any check requires fixes, restart from lint. Success means the codebase is stable across all quality gates.

---

## Quality Pipeline

| Step | Make Command | Scope |
|------|--------------|-------|
| lint | `make lint` | formatting, style, file size |
| check | `make check` | test failures |
| sanitize | `make check-sanitize` | ASan/UBSan memory errors |
| tsan | `make check-tsan` | data races, thread issues |
| valgrind | `make check-valgrind` | memory leaks, invalid access |
| helgrind | `make check-helgrind` | lock ordering, thread errors |
| coverage | `make coverage` | missing test coverage |

## Execution

### Main Loop

```
iteration = 0
loop:
  iteration++
  report: "=== Quality iteration {iteration} ==="

  for each (step_name, make_command) in pipeline:
    report: "Running {step_name}..."
    result = spawn_subagent(step_name, make_command)

    if result.status == "stuck":
      HALT - report issue to user and wait for input

    if result.status == "fixed":
      report: "{step_name} fixed issues, restarting from lint"
      break  # restart loop from beginning

  if all steps returned "clean":
    SUCCESS - report clean run and stop
```

### Spawning Sub-Agents

**Tool Configuration:**
- Tool: `Task`
- `subagent_type`: `general-purpose`
- `model`: `sonnet`

**CRITICAL:** Run sub-agents ONE AT A TIME. Wait for each to complete before spawning the next.

**Prompt Template:**

```
## Context

**For lint, check, sanitize, tsan, valgrind, helgrind steps:**
Use `/persona developer` to load the developer skill set.

**For coverage step:**
Use `/persona coverage` to load the coverage skill set.

## Task

Run `{MAKE_COMMAND}` and fix any issues found.

**Scope:** Only fix issues detected by this specific check ({STEP_NAME}).

## Process

1. Run `{MAKE_COMMAND}`
2. If it passes with no issues: report status "clean"
3. If issues found:
   - Analyze and fix them
   - Re-run `{MAKE_COMMAND}` to verify
   - If fixed: commit changes with message "{STEP_NAME}: fix [brief description]"
   - If unable to fix after reasonable attempts: report status "stuck"

## Lint-Specific Guidance (for lint step only)

**Filesize violations are fixable.** When a file exceeds the size limit:

1. **Analyze the file** - Identify logical groupings of functions
2. **Extract to new module** - Move related functions to a new `.c` file with matching `.h`
3. **Update includes** - Add `#include` for the new header where needed
4. **Update Makefile** - Add the new `.c` file to the appropriate `SRCS` or `TEST_SRCS` variable
5. **Verify** - Run `make check` to ensure nothing broke

**Splitting strategies:**
- Group by data structure (e.g., `foo_render.c` for rendering functions)
- Group by operation type (e.g., `foo_parse.c` for parsing functions)
- Extract helper/utility functions to a `foo_utils.c`
- For test files: split by test category or feature area

**This IS your job.** Do not report "stuck" for filesize violations. Splitting files into logical modules is exactly what the lint check exists to enforce.

## Required Response Format

End your response with EXACTLY this JSON block:

```json
{"status": "clean|fixed|stuck", "message": "brief description"}
```

Status meanings:
- "clean": Check passed, no changes needed
- "fixed": Found and fixed issues, committed changes
- "stuck": Found issues but unable to resolve them

If stuck, explain what you tried and what's blocking progress.
```

### Interpreting Results

Parse the JSON from the sub-agent's response:
- `status: "clean"` → proceed to next step
- `status: "fixed"` → restart loop from lint (iteration++)
- `status: "stuck"` → HALT, report to user, wait for instructions

### Success Criteria

The pipeline succeeds when a complete iteration runs through all 7 steps with every sub-agent returning `status: "clean"`.

Report: "Quality gate passed: clean run through all checks on iteration {N}"

Then wait for instructions.
