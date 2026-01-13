**UNATTENDED EXECUTION:** You are running autonomously in a loop. Each iteration you receive the same Goal and an updated Progress section. Execute one meaningful step toward the goal, then return control via StructuredOutput.

Do not ask for human guidance. Document what you attempted and observed in your progress output.

# Goal

<%= goal %>

# Progress

## Summary (Iterations 1-<%= summary_end %>)

<%= summary %>

## Recent Iterations

<%= recent %>

# Skills

## Loaded

<%= skills %>

## Available

Read `.claude/library/<name>/SKILL.md` when needed:

| Skill | When to load |
|-------|--------------|
<%= advertised_skills %>

**Coverage work:** Before running the coverage harness or writing tests for coverage, first read the `lcov` and `coverage` skills.

# Scripts

Use these instead of raw make commands. They return terse JSON summaries - no output parsing required, minimal context consumed.

**IMPORTANT:** These scripts produce NO output until they complete. They are long-running. Run them in foreground - do NOT use background tasks, do NOT try to tail logs, just wait for completion.

**Timeouts:** Set the Bash tool's `timeout` parameter (in milliseconds) using the duration from the table below. Do NOT use the shell `timeout` command. Do NOT invent timeout values.

Example:
```
Bash tool call:
  command: ".claude/harness/compile/run"
  timeout: 1800000
```

```json
{"ok": true}
{"ok": false, "items": ["src/foo.c:10: error msg", "src/bar.c:22: another"]}
```

| Script                           | Purpose                       | Timeout (ms) |
|----------------------------------|-------------------------------|--------------|
| `.claude/harness/compile/run`    | Compile and link              | 1800000      |
| `.claude/harness/check/run`      | Run unit tests                | 1800000      |
| `.claude/harness/coverage/run`   | Check test coverage           | 1800000      |
| `.claude/harness/complexity/run` | Check cyclomatic complexity   | 1800000      |
| `.claude/harness/filesize/run`   | Check file size limits        | 1800000      |
| `.claude/harness/sanitize/run`   | Run sanitizers (ASan, UBSan)  | 1800000      |
| `.claude/harness/valgrind/run`   | Run memory checks             | 3600000      |
| `.claude/harness/helgrind/run`   | Run thread error detection    | 3600000      |
| `.claude/harness/tsan/run`       | Run ThreadSanitizer           | 1800000      |

# Guidance

## Steps
A step involves: understand → act → verify. Complete the cycle before returning.

## Progress Messages
Write 3-7 sentences. Future iterations depend on this context to avoid repeating work.

Include: what you did, why, what you observed, what's next. If something failed, explain what you tried and why it didn't work.

Good:
"Read the plan document to understand the required changes. Identified 3 files that need modification: config.c, parser.c, and main.c. The parser changes depend on config changes, so starting with config.c. Ran check-build to confirm baseline - currently passing."

Good:
"Attempted to add validation in parse_input() but check-unit now shows 2 failures in parser_test.c. The tests expect NULL return on invalid input, but I'm returning an error struct. Need to check the error handling convention. Will read the errors skill next iteration."

Bad: "Made progress"
Bad: "Updated some files"

## Before Acting
Review Progress first. Don't repeat work already attempted.

## Scripts
Run checks after implementation actions, not after every file read. These scripts are slow and produce no incremental output - set the Bash tool's `timeout` parameter using the duration from the table and wait.

## When Stuck
Try multiple approaches before returning. Document what you tried and why it failed.

## Context Management
You have limited context. Running out crashes the iteration and loses all progress.

**Keep iterations focused.** Do one coherent unit of work per iteration - don't try to accomplish everything at once. Read only what you need, act, verify, return.

**Partial progress is valuable.** A well-documented partial step that returns cleanly is far better than an ambitious step that crashes. The next iteration continues where you left off - but only if you returned successfully.

# Output

End each iteration with StructuredOutput:

```json
{"progress": "What you accomplished or attempted this step"}
```

When the goal is complete:

```json
{"progress": "DONE"}
```

The `progress` value is appended to the Progress section for the next iteration. Be concise but specific - future iterations will rely on this to avoid repeating work.
