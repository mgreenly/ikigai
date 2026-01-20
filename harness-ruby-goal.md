# Harness Ruby Migration Goal

## Objective

Convert 10 Python harness check scripts to Ruby, removing fix functionality while maintaining identical JSON output behavior and improving parallelization.

## Reference

**Project Context:**
- `CLAUDE.md` - Project rules (never change directories, use jj not git, no parallel make with different targets)
- `.claude/README.md` - Harness system architecture

**Parsing Analysis:**
Agent summaries from conversation provide complete parsing logic for each harness:
- **compile** - Regex on gcc/ld errors: `file:line:col: error: message`
- **filesize** - Regex on emoji output: `file N bytes (limit M)`
- **unit** - XML parsing from `reports/check/unit/*.xml`: `file:line function: message`
- **integration** - Emoji markers + text parsing: `test_path: integration test failed`
- **complexity** - Regex on pmccabe: `file:function complexity=N` or `file:function nesting=N`
- **sanitize** - Emoji markers → re-run individual tests → parse ASan/UBSan/LSan output
- **tsan** - Emoji markers → re-run individual tests → parse ThreadSanitizer output
- **valgrind** - Emoji markers → re-run individual tests → parse Valgrind output
- **helgrind** - Emoji markers → re-run individual tests → parse Helgrind output
- **coverage** - Parse `reports/coverage/summary.txt`: `file lines:X% funcs:Y% branches:Z%`

**Existing Python Scripts:**
- `.claude/harness/compile/run`
- `.claude/harness/filesize/run`
- `.claude/harness/unit/run`
- `.claude/harness/integration/run`
- `.claude/harness/complexity/run`
- `.claude/harness/sanitize/run`
- `.claude/harness/tsan/run`
- `.claude/harness/valgrind/run`
- `.claude/harness/helgrind/run`
- `.claude/harness/coverage/run`

## Outcomes

**Conversion Order (complete in sequence):**
1. `check-compile` → `.claude/harness/compile/run.rb`
2. `check-filesize` → `.claude/harness/filesize/run.rb`
3. `check-unit` → `.claude/harness/unit/run.rb`
4. `check-integration` → `.claude/harness/integration/run.rb`
5. `check-complexity` → `.claude/harness/complexity/run.rb`
6. `check-sanitize` → `.claude/harness/sanitize/run.rb`
7. `check-tsan` → `.claude/harness/tsan/run.rb`
8. `check-valgrind` → `.claude/harness/valgrind/run.rb`
9. `check-helgrind` → `.claude/harness/helgrind/run.rb`
10. `check-coverage` → `.claude/harness/coverage/run.rb`

**Shared Library:**
- Create `.claude/harness/lib/` directory
- Shared modules for common functionality (make execution, JSON output, etc.)
- Use `require_relative '../lib/module_name'` to load

**Each Ruby Script Must:**
1. **Output only JSON** - `{"ok": true}` or `{"ok": false, "items": ["error1", ...]}`
2. **No accidental output** - All subprocess output captured, never printed
3. **Single flag** - `--file <path>` only (remove `--fix`, `--verbose`, `--time-out`)
4. **Correct exit codes** - 0 on success, non-zero on failure
5. **Shebang** - `#!/usr/bin/env ruby` at top

**Parallelization Strategy:**

When `--file` is specified: Run once directly (no parallel logic needed)

When `--file` is NOT specified:

*Text-based checks (compile, filesize, complexity, coverage):*
- Run with `MAKE_JOBS=<cores/2>` first (fast)
- If exit code != 0, re-run with `MAKE_JOBS=1` (clean output for parsing)

*XML-based checks (unit):*
- Run with `MAKE_JOBS=<cores/2>`
- Parse XML files directly (no re-run needed)

*Two-phase test harnesses (integration, sanitize, tsan, valgrind, helgrind):*
- Run with `MAKE_JOBS=<cores/2>`
- Parse emoji markers from output
- Re-run individual failing tests to get detailed errors
- No serial re-run of full make target needed

**File Filtering (--file flag):**
- Validate file exists early
- For test harnesses: Map source file to relevant tests
- For other checks: Filter parsed results to target file
- Return `{"ok": true}` if file has no errors

**Symlink Updates:**
- Update `.claude/scripts/check-*` symlinks to point to `run.rb` instead of `run`
- Leave old Python `run` scripts in place (for reference/rollback)

## Acceptance

**Per Script:**
- Ruby script outputs identical JSON schema to Python version
- No output to stdout except final JSON
- No output to stderr
- `--file` flag works correctly
- Parallelization logic works as specified
- Exit codes match success/failure state

**Overall:**
- All 10 scripts converted and working
- Symlinks updated
- Can successfully run: `.claude/scripts/check-compile`, `.claude/scripts/check-unit`, etc.
- JSON output is valid and parseable

**Manual Verification:**
Run each check script and verify JSON output format is correct.
