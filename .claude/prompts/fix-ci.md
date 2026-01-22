/skillset meta

## Context: Fixing CI Test Failures

PR #50 on mgreenly/ikigai - CI fails at check-unit with 11 tests failing.

### The Problem
Tests pass when run individually (debug step passes) but fail when run via `make check-unit` which runs tests in parallel via xargs -P.

### Failing Tests (11 total)
1. **VCR-based tests (6)** - use vcr_init() to replay HTTP fixtures:
   - tests/unit/providers/anthropic/anthropic_streaming_{async,basic,advanced}_test
   - tests/unit/providers/google/google_streaming_{content,advanced,async}_test

2. **Tool tests (4)** - exec binaries via fork/execl:
   - tests/unit/tools/{file_read,grep,file_edit,file_write}_test

3. **Database test (1)**:
   - tests/unit/db/zzz_migration_errors_test

### Key Files
- `.github/workflows/ci.yml` - CI workflow
- `.make/check-unit.mk` - test execution (line 35: parallel xargs)
- `tests/helpers/vcr_helper.c` - VCR fixture loading (line 68-86: fixture path)
- Test binaries use relative paths:
  - VCR: `tests/fixtures/vcr/{provider}/{test_name}.jsonl`
  - Tools: `libexec/ikigai/{tool}-tool`

### What Works
- OpenAI streaming tests (no VCR) pass
- All tests pass when run individually in debug step
- check-compile, check-link, check-filesize all pass

### Hypotheses to Investigate
1. **Parallel execution race condition** - tests work individually but not in parallel
2. **XML report generation issue** - check-unit marks tests failed if no XML file exists
3. **Working directory issue** - relative paths might differ when run via xargs
4. **Resource contention** - VCR file handles, DB connections, or process spawning

### Next Steps
1. Check if XML files are being generated in CI (add `ls -la reports/check/...` after tests)
2. Try running tests serially (`xargs -P1` instead of `-P$(MAKE_JOBS)`)
3. Check if VCR tests have working directory assumptions
4. Verify tool binary paths resolve correctly in parallel context

### Commands
- View PR: `gh pr view 50 --repo mgreenly/ikigai`
- Re-run CI: `gh pr close 50 && gh pr reopen 50 --repo mgreenly/ikigai`
- Check logs: `gh run view --log-failed --job=<JOB_ID> --repo mgreenly/ikigai`
