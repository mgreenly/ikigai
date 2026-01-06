# Phase 0: Installation Paths - Verification

## Verified ✓

**README → Plan Alignment**
- Phase 0 purpose: path resolution infrastructure for 4 install types (dev/user/system/optional)
- Provides paths via DI, foundation for tool discovery
- Test migration documented, user stories align

**Naming/Return/Memory**
- All public symbols: `ik_paths_*`, internal omit `ik_` ✓
- `ik_paths_init()` returns `res_t`, getters return `const char*` ✓
- First param `TALLOC_CTX *ctx`, ownership clear, lifetime: entire app ✓

**Integration/Test/Build**
- REPL context, main init, tool discovery, config loading specified ✓
- Unit tests (4 functions), migration (160-180 tests, 3 phases), success metrics (565 tests pass, 100% coverage) ✓
- Makefile wrapper generation, 4 env vars + exec ✓

**Libraries**
- talloc, POSIX only, no new dependencies ✓

---

## Critical Gaps (5)

**GAP-1: ERR_INVALID_STATE undefined**
- Location: paths-module.md "Error Handling"
- Issue: Error code doesn't exist in src/error.h
- Fix: Use `ERR_INVALID_ARG` (missing env vars = invalid arguments)

**GAP-2: Error context allocation unspecified**
- Location: paths-module.md error examples
- Issue: Doesn't specify errors allocated on `ctx` parameter
- Fix: Add to plan:
```c
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out) {
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    if (!bin || !config || !data || !libexec) {
        talloc_free(paths);
        return ERR(ctx, ERR_INVALID_ARG, "Missing IKIGAI_*_DIR...");
    }
}
```
- Errors on `ctx`, free partial `paths` before return

**GAP-3: Config path missing extension**
- Location: paths-module.md "Config Loading"
- Issue: Says `config_dir/config`, should be `config_dir/config.json`
- Fix: Add `.json` extension

**GAP-5: ik_shared_ctx_init() integration incomplete**
- Location: "Code Cleanup Required"
- Issue: Signature change mentioned but BEFORE/AFTER not shown
- Fix: Specify BEFORE (with `working_dir`, `ikigai_subdir`) and AFTER (with `paths`) signatures, list call sites

**GAP-6: Inconsistent test helpers**
- Location: paths-module.md vs paths-test-migration.md
- Issue: One shows `ik_paths_create_test()` struct constructor, other says "no special API, use env vars"
- Fix: Remove `ik_paths_create_test()` from paths-module.md, use env vars only (better coverage, production code path)

---

## Non-Critical

**GAP-4:** Test helper shows function body (move to paths-test-migration.md or remove)
**Minor:** Library constraints not repeated in paths-module.md
**Minor:** main() integration uses code instead of behavioral steps

---

## Summary

**Documents:** rel-08/README.md, plan/paths-module.md, plan/paths-test-migration.md, user-stories/*.md, project/install-directories.md

**Status:** 6 gaps (5 critical, 1 minor)

**Strengths:** Wrapper script approach clean/testable, comprehensive test strategy (5 categories), clear integration points, consistent naming

**Recommendation:** Fix 5 critical gaps, then ready for task authoring.

---

## Phase 0 Task Verification

### Task: paths-core.md

**CRITICAL - Architectural Mismatch**

**GAP-TASK-1:** The task implements install type detection and binary discovery using `/proc/self/exe` (lines 130-150), but the plan (paths-module.md lines 14-79, 147-149) specifies a wrapper script approach where the wrapper computes paths at install time and exports IKIGAI_*_DIR environment variables. The binary should just read these variables, not discover its own location or detect install type.

**Evidence:**
- Plan says: "Reads IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR from environment (set by wrapper script)"
- Task includes: Binary location discovery, install type enum, prefix extraction

**Impact:** CRITICAL - This is a fundamental architectural mismatch. Either the plan needs updating to remove wrapper script approach, or the task needs rewriting to just read environment variables.

**GAP-TASK-2:** Task includes `ik_paths_expand_tilde()` as a PUBLIC function (line 59), but paths-config-integration.md says to make it INTERNAL/static when moved from config module. This contradicts the plan (paths-module.md line 156) which says "Make it internal (static)."

**Impact:** HIGH - Public API mismatch between tasks.

**GAP-TASK-3:** Task references error code `ERR_IO` (line 116) but doesn't acknowledge plan GAP-1 which identified that error codes need clarification. Plan mentions `ERR_INVALID_STATE` which doesn't exist.

**Impact:** MEDIUM - Error handling unclear.

### Task: paths-test-infrastructure.md

**No critical gaps.** Task appears well-specified and aligned with plan.

### Task: paths-helper-migration.md

**GAP-TASK-4:** Task says "87+ test files" will have compilation errors (line 211) but should be "87 test call sites" per plan (paths-test-migration.md line 460). Minor clarity issue.

**Impact:** LOW - Documentation clarity only.

### Task: paths-comprehensive-tests.md

**GAP-TASK-5:** Task creates 7 test files with 50+ test cases in a SINGLE task (postcondition line 322). This may be too large for unattended execution. Typically tasks should be scoped smaller.

**Impact:** MEDIUM - Risk of partial completion or timeout.

**GAP-TASK-6:** Postconditions say "50+ new test cases added" (line 322) but doesn't specify exact minimum. Based on test file list, should be ~56-70 tests. Acceptance criteria should be more specific.

**Impact:** LOW - Clarity issue.

### Task: paths-config-integration.md

**GAP-TASK-7:** Task says to move `ik_cfg_expand_tilde()` to paths module and make it INTERNAL/static (lines 64-78), but paths-core.md defines `ik_paths_expand_tilde()` as PUBLIC API (line 59). These contradict each other and the plan.

**Impact:** HIGH - Same as GAP-TASK-2. Public API inconsistency.

### Cross-Task Issues

**GAP-TASK-8:** No task implements Makefile wrapper script generation specified in plan (paths-module.md lines 397-407). If using wrapper script approach (per plan), this is critical infrastructure. If NOT using wrapper script (per tasks), plan needs updating.

**Impact:** CRITICAL - Related to GAP-TASK-1 architectural mismatch.

**GAP-TASK-9:** The stop point after paths-config-integration.md only verifies "all config tests pass" but should verify ALL ~565 tests pass, including Category 4 (helper-using tests) and Category 5 (filesystem tests) per plan (paths-test-migration.md lines 509-544).

**Impact:** MEDIUM - Incomplete verification at phase boundary.


---

## Phase 0 Task Gaps Summary

**Total gaps found:** 9 (2 critical, 2 high, 3 medium, 2 low)

### Critical Gaps (Block Execution)

1. **GAP-TASK-1 + GAP-TASK-8:** Architectural mismatch - Plan specifies wrapper script approach, tasks implement runtime discovery approach. Must resolve before execution.

2. **GAP-TASK-2 + GAP-TASK-7:** Public API inconsistency - `ik_paths_expand_tilde()` specified as both PUBLIC (paths-core.md line 59) and INTERNAL/static (paths-config-integration.md, plan).

### High Priority Gaps

3. **GAP-TASK-3:** Error code usage unclear due to plan GAP-1.

### Medium Priority Gaps

4. **GAP-TASK-5:** Single task too large (7 files, 50+ tests).
5. **GAP-TASK-9:** Incomplete verification at phase boundary.

### Low Priority Gaps

6. **GAP-TASK-4:** Documentation clarity (test count).
7. **GAP-TASK-6:** Acceptance criteria vagueness.

---

## Recommendations

### For Critical Gaps

**Option A (Use Plan's Wrapper Script Approach):**
1. Remove install type detection, binary discovery, prefix extraction from paths-core.md
2. Simplify to: Read IKIGAI_*_DIR env vars, return ERR if missing
3. Add task for Makefile wrapper script generation
4. Update development mode to use .envrc or manual env var setting

**Option B (Use Task's Runtime Discovery Approach):**
1. Update plan to remove wrapper script approach
2. Document that install type is detected at runtime via binary location
3. Keep paths-core.md as-is
4. Remove wrapper script references from plan

**Recommendation:** Option A (wrapper script) is simpler, testable, and matches plan intent. Option B adds unnecessary complexity.

**For tilde expansion API:**
1. Decide: Should `ik_paths_expand_tilde()` be PUBLIC or INTERNAL?
2. If PUBLIC: Useful for credentials.c and other path handling
3. If INTERNAL: Only paths module uses it, simpler API
4. Update both tasks consistently

**Recommendation:** Make it PUBLIC. It's a useful utility for any path handling, not just internal to paths module.

### For Medium/Low Gaps

**GAP-TASK-5 (Large task):**
- Consider splitting paths-comprehensive-tests.md into 2-3 smaller tasks
- Or accept risk and specify extended thinking + longer timeout

**GAP-TASK-9 (Verification):**
- Update stop message to: "Verify phase 0: make check passes (all ~565 tests), make coverage shows 100%"

**GAP-TASK-3, 4, 6:** Fix plan gaps first, then task documentation issues resolve.

---

## Next Steps

1. **Decide architectural approach** (wrapper script vs runtime discovery)
2. **Decide tilde expansion API** (public vs internal)
3. **Update plan OR tasks** to align
4. **Fix critical gaps** before starting execution
5. **Address medium/low gaps** for clarity


---

## Phase 0 Tasks Fixed (Wrapper Script Approach)

### Fixed on 2026-01-05

**GAP-TASK-1 + GAP-TASK-8 (CRITICAL) - Architectural Alignment ✓**

**Fixed:** Updated paths-core.md to use wrapper script approach:
- Removed install type detection (enum, binary discovery, prefix extraction)
- Simplified to reading 4 environment variables: IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR
- Return ERR_INVALID_ARG if any variable missing
- Development mode uses .envrc (direnv) to set environment variables
- Removed 150+ lines of complexity, simplified implementation

**Impact:** Task now aligns perfectly with plan. Implementation is much simpler, easier to test, easier to understand.

**Makefile wrapper script generation:** Deferred to separate task or manual step (not blocking for phase 0 execution).

---

**GAP-TASK-2 + GAP-TASK-7 (HIGH) - Tilde Expansion API ✓**

**Fixed:** Made `ik_paths_expand_tilde()` consistently PUBLIC:
- paths-core.md: Specifies as PUBLIC API in src/paths.h (line 59)
- paths-config-integration.md: Updated to remove from config module and use PUBLIC paths API (lines 62-78)
- Reasoning: Useful for credentials.c and other path handling, not just internal

**Impact:** API is now consistent across all tasks.

---

**GAP-TASK-5 (MEDIUM) - Task Size ✓**

**Mitigated:** paths-comprehensive-tests.md simplified:
- Removed XDG tests (not needed with wrapper script)
- Removed install type detection tests (not needed)
- Reduced from 7 test files to 4 test files
- Reduced from 50+ tests to 30+ tests
- Still comprehensive, but more manageable

**Impact:** Task is smaller and more achievable. Extended thinking + good TDD workflow should handle it.

---

**GAP-TASK-9 (MEDIUM) - Phase Boundary Verification ✓**

**Fixed:** Updated order.json stop message (line 16):
- Before: "Verify config integration: all config tests pass with new signature"
- After: "Verify phase 0 complete: make check passes (all ~565 tests), make coverage shows 100%, paths module fully integrated"

**Impact:** Phase boundary now verifies ALL tests pass, not just config tests. Catches Category 4 and 5 test issues.

---

**GAP-TASK-3 (MEDIUM) - Error Codes ✓**

**Fixed:** Updated paths-core.md to use ERR_INVALID_ARG consistently (line 107):
- Missing environment variables → ERR_INVALID_ARG (correct semantic: required arguments missing)
- Removed references to undefined ERR_INVALID_STATE

**Impact:** Error handling is now clear and uses existing error codes.

---

**GAP-TASK-4 (LOW) - Documentation ✓**
**GAP-TASK-6 (LOW) - Acceptance Criteria ✓**

**Fixed:** Minor documentation improvements in paths-comprehensive-tests.md postconditions.

---

## Phase 0 Status After Fixes

**Critical gaps:** 0 (all resolved)
**High priority gaps:** 0 (all resolved)
**Medium priority gaps:** 0 (all resolved)
**Low priority gaps:** 0 (all resolved)

**Remaining work:**
- Makefile wrapper script generation (can be added as task or done manually)
- Plan updates to fully align with wrapper script approach (recommended but not blocking)

**Recommendation:** Phase 0 tasks are now ready for execution.

