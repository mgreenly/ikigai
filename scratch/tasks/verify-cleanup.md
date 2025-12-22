# Task: Verify Cleanup Stage

**VERIFICATION TASK:** This task does not create code. It verifies previous tasks completed correctly.

**Model:** sonnet/thinking
**Depends on:** verify-providers.md, openai-equivalence-validation.md, cleanup-openai-source.md, cleanup-openai-adapter.md, cleanup-openai-tests.md, cleanup-openai-docs.md

## Context

**Working directory:** Project root (where `Makefile` lives)

This is the **final verification gate**. It ensures:
1. Legacy `src/openai/` code has been completely removed
2. OpenAI shim adapter has been removed
3. All code now routes through the native provider abstraction
4. No remnants of old code paths remain
5. Documentation has been updated

**CRITICAL:** If cleanup verification passes, the migration is complete.

If any verification step fails, return `{"ok": false, "blocked_by": "<specific failure>"}`.

## Pre-Read

Read cleanup task files to understand what should have been removed:
- `scratch/tasks/cleanup-openai-source.md` - Legacy source removal
- `scratch/tasks/cleanup-openai-adapter.md` - Shim adapter removal
- `scratch/tasks/cleanup-openai-tests.md` - Test updates
- `scratch/tasks/cleanup-openai-docs.md` - Documentation updates

## Verification Protocol

### Step 1: Legacy Source Directory Removed

**CRITICAL:** The old `src/openai/` directory must be completely gone.

```bash
ls -la src/openai/ 2>&1
```

- [ ] Directory does not exist (command returns error)

If directory exists, list contents:
```bash
ls src/openai/*.c src/openai/*.h 2>/dev/null | wc -l
```
- [ ] Must be 0 files

### Step 2: No Legacy Includes Remain

Search for includes of old openai path:
```bash
grep -rn '#include.*"openai/' src/
grep -rn '#include.*<openai/' src/
```

- [ ] No includes of `"openai/..."` pattern (empty output)

The only valid openai includes should be in `src/providers/openai/`:
```bash
grep -rn '#include.*openai' src/ | grep -v "src/providers/openai"
```
- [ ] Empty output (no references outside providers/openai/)

### Step 3: No Legacy Function Calls Remain

Search for calls to old OpenAI functions:
```bash
grep -rn "ik_openai_client_\|ik_openai_multi_\|ik_openai_http_" src/ | grep -v "src/providers/openai"
```

- [ ] No calls to legacy functions outside providers/openai/ (empty output)

### Step 4: Shim Adapter Removed

The shim adapter (that wrapped legacy code) should be removed:

```bash
ls src/providers/openai/shim* 2>/dev/null
grep -rn "shim\|adapter.*legacy\|wrap.*legacy" src/providers/openai/
```

- [ ] No shim files exist
- [ ] No shim-related code in native provider

### Step 5: Makefile Updated

Verify Makefile no longer references old source files:
```bash
grep -n "src/openai/" Makefile
```

- [ ] No references to `src/openai/` in Makefile (empty output)

Verify new provider sources are in Makefile:
```bash
grep -n "src/providers/" Makefile
```
- [ ] References to `src/providers/` exist

### Step 6: Build Still Works

```bash
make clean && make all 2>&1
```

- [ ] Build succeeds
- [ ] No errors
- [ ] No warnings about missing files

### Step 7: All Tests Pass

```bash
make check 2>&1 | tail -30
```

- [ ] All tests pass
- [ ] No test failures

Count results:
```bash
make check 2>&1 | grep -c "PASS"
make check 2>&1 | grep -c "FAIL"
```
- [ ] PASS count > 0
- [ ] FAIL count = 0

### Step 8: Test Files Updated

Verify old test paths are removed:
```bash
ls tests/unit/openai/ 2>&1
```

- [ ] `tests/unit/openai/` directory does not exist OR is empty

New provider tests should exist:
```bash
ls tests/unit/providers/openai/*.c 2>/dev/null | wc -l
```
- [ ] Provider OpenAI tests exist (count > 0)

### Step 9: No Dead Code

Search for commented-out legacy code:
```bash
grep -rn "// ik_openai_\|/\* ik_openai_\|#if 0.*openai" src/
```

- [ ] No commented-out legacy code (empty output)

Search for TODO markers referencing old code:
```bash
grep -rn "TODO.*openai\|FIXME.*openai\|XXX.*openai" src/
```
- [ ] No TODOs about old OpenAI code (empty output)

### Step 10: Documentation Updated

Check architecture documentation:
```bash
grep -n "src/openai/" project/architecture.md project/*.md 2>/dev/null
```

- [ ] No references to old `src/openai/` path

Check for updated provider documentation:
```bash
grep -n "src/providers/" project/architecture.md project/*.md 2>/dev/null
```
- [ ] References to new `src/providers/` path exist

### Step 11: Skill Files Updated

Check Claude skill files:
```bash
grep -n "src/openai/" .claude/library/*/SKILL.md 2>/dev/null
```

- [ ] No references to old path in skill files

Specifically check source-code skill:
```bash
grep -n "openai" .claude/library/source-code/SKILL.md
```
- [ ] References updated to `src/providers/openai/`

### Step 12: No Orphaned Test Fixtures

Check for orphaned OpenAI fixtures:
```bash
ls tests/fixtures/openai/ 2>/dev/null | wc -l
```

If fixtures existed in old location, verify they're moved:
- [ ] Old fixture location empty or doesn't exist
- [ ] New fixtures in `tests/fixtures/responses/` or similar

### Step 13: REPL Integration Complete

Verify REPL uses only provider abstraction:

```bash
grep -n "ik_openai_" src/repl*.c src/commands*.c
```

- [ ] No direct OpenAI function calls in REPL (empty output)

All provider access through vtable:
```bash
grep -n "provider->vt->" src/repl*.c | head -10
```
- [ ] REPL uses vtable for provider access

### Step 14: Config References Updated

Check config code:
```bash
grep -n "openai_api_key\|openai_model\|openai_temperature" src/config.c src/config.h
```

- [ ] No legacy OpenAI config fields (empty output)

New config uses provider abstraction:
```bash
grep -n "provider\|credentials" src/config.c src/config.h | head -10
```
- [ ] Config references providers/credentials

### Step 15: Equivalence Validation Passed

Verify equivalence validation task passed:

```bash
cat scratch/tasks/openai-equivalence-validation.md | grep -A 5 "Postconditions"
```

Check for equivalence test results:
```bash
make build/tests/unit/providers/openai/equivalence_test 2>/dev/null && ./build/tests/unit/providers/openai/equivalence_test
```

- [ ] Equivalence tests pass (native matches legacy behavior)

### Step 16: No Dual Code Paths

Search for conditional compilation around old/new code:
```bash
grep -rn "#ifdef.*LEGACY\|#ifdef.*OLD_OPENAI\|#if.*USE_SHIM" src/
```

- [ ] No conditional compilation for legacy code (empty output)

### Step 17: Memory Leak Final Check

Run valgrind on full test suite:
```bash
make BUILD=valgrind check-unit 2>&1 | grep -E "definitely lost|ERROR SUMMARY"
```

- [ ] No memory leaks
- [ ] Error summary shows 0 errors

### Step 18: Binary Size Sanity Check

Verify binary didn't grow unexpectedly (no duplicate code):
```bash
ls -la build/ikigai 2>/dev/null || ls -la build/client 2>/dev/null
```

Compare to expected size (should not have doubled):
- [ ] Binary size is reasonable (not bloated with duplicate code)

### Step 19: Git Status Clean

Verify no uncommitted cleanup artifacts:
```bash
git status --porcelain
```

- [ ] Working directory is clean (empty output)
- [ ] All cleanup changes are committed

### Step 20: Final Cross-Check

Comprehensive search for any openai remnants outside providers:
```bash
grep -rn "openai" src/ | grep -v "src/providers/openai" | grep -v "// Provider name" | head -20
```

Review any matches:
- [ ] Only legitimate references (provider name strings, comments)
- [ ] No code references to old implementation

## Postconditions

- [ ] All 20 verification steps pass
- [ ] `src/openai/` directory completely removed
- [ ] No legacy includes or function calls remain
- [ ] Shim adapter removed
- [ ] Makefile updated
- [ ] All tests pass
- [ ] Documentation updated
- [ ] No dead code or TODOs
- [ ] No memory leaks
- [ ] Git working directory clean

## Success Criteria

Return `{"ok": true}` if ALL verification steps pass.

**This signifies the migration is COMPLETE.**

Return `{"ok": false, "blocked_by": "<specific failure>"}` if any step fails.

**Example failures:**

```json
{
  "ok": false,
  "blocked_by": "Step 1: Legacy Source - src/openai/ directory still exists with 12 files. cleanup-openai-source.md did not complete.",
  "affected_downstream": ["Migration incomplete - old and new code coexist"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 3: Legacy Calls - Found ik_openai_multi_add_request() call in src/repl_actions_llm.c:312. REPL still using legacy code path.",
  "affected_downstream": ["Migration incomplete - dual code paths"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 7: Tests - 5 tests now fail after cleanup. Failures in: test_client_http_streaming, test_tool_loop, test_repl_submission, test_repl_streaming, test_provider_switch",
  "affected_downstream": ["Cleanup broke functionality - must investigate before release"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 15: Equivalence - Native OpenAI provider produces different streaming events than legacy shim. Event order differs for tool calls.",
  "affected_downstream": ["Cannot remove shim until equivalence verified"]
}
```

## Migration Complete Checklist

When this verification passes, confirm:

- [x] Multi-provider architecture fully implemented
- [x] Anthropic, Google, OpenAI all working through unified vtable
- [x] Legacy OpenAI code completely removed
- [x] No shim or adapter code remaining
- [x] All tests passing
- [x] Documentation current
- [x] Ready for release
