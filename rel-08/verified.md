# Verification Log

Items verified and gaps found during plan verification.

## Phase 0: Installation Paths

### Verified Items

**README → Plan Alignment**
- ✓ Phase 0 purpose matches: path resolution infrastructure
- ✓ Supports detection of 4 install types (development/user/system/optional)
- ✓ Provides paths via dependency injection
- ✓ Foundation for tool discovery
- ✓ Test migration strategy documented
- ✓ User stories align (add-custom-tool.md mentions ~/.ikigai/tools/)

**Naming Conventions**
- ✓ All public symbols follow ik_paths_* pattern
- ✓ Internal symbols correctly omit ik_ prefix
- ✓ Module name is single word: "paths"

**Return Value Conventions**
- ✓ ik_paths_init() returns res_t (can fail)
- ✓ Getters return const char* (query pattern)
- ✓ Uses OK()/ERR() pattern

### Gaps Found

**GAP-1: Undefined error code ERR_INVALID_STATE**

**Location:** paths-module.md, "Error Handling" section

**Issue:** Plan uses ERR_INVALID_STATE for missing environment variables, but this error code does not exist in src/error.h. Current error codes from errors skill:
- ERR_INVALID_ARG, ERR_OUT_OF_RANGE, ERR_IO, ERR_PARSE, ERR_DB_CONNECT, ERR_DB_MIGRATE, ERR_OUT_OF_MEMORY, ERR_AGENT_NOT_FOUND, ERR_PROVIDER, ERR_MISSING_CREDENTIALS, ERR_NOT_IMPLEMENTED

**Resolution needed:**
- Option A: Use ERR_INVALID_ARG instead (invalid argument = missing required env var)
- Option B: Add ERR_INVALID_STATE to src/error.h with specific numeric value (e.g., 12)
- Plan should specify which approach to take

**Recommendation:** Use ERR_INVALID_ARG - missing environment variables are effectively invalid arguments to the init function. More specific than ERR_IO.

**Memory Management**
- ✓ First parameter is TALLOC_CTX *ctx
- ✓ Ownership clearly stated (instance lives on root_ctx)
- ✓ Child allocation specified (strings allocated as children)
- ✓ Lifetime documented (entire application)

**GAP-2: Error context allocation not specified**

**Location:** paths-module.md, error handling code examples

**Issue:** Plan doesn't specify which context errors should be allocated on. According to error context lifetime rules:
- Errors must be allocated on a context that survives the return
- When ik_paths_init() encounters missing env vars, it allocates paths on ctx but hasn't assigned *out yet
- If error is allocated on paths, and paths is freed, use-after-free
- If error is allocated on paths, and paths is NOT freed, memory waste

**Resolution needed:** Specify that errors are allocated on ctx parameter (Option A from errors skill):
```c
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out) {
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    // ... read env vars ...
    if (!bin || !config || !data || !libexec) {
        talloc_free(paths);  // Clean up partial struct
        return ERR(ctx, ERR_INVALID_ARG, "Missing IKIGAI_*_DIR environment variables");
    }
    // ... success path ...
}
```

**Recommendation:** Add to plan: "All errors in ik_paths_init() are allocated on ctx parameter. Partial paths struct is freed before returning error."

**Integration Points**
- ✓ REPL Context field addition shown
- ✓ Main initialization flow documented
- ✓ Tool discovery integration specified
- ✓ Config loading signature change shown

**GAP-3: Config file path inconsistency**

**Location:** paths-module.md, "Config Loading" section

**Issue:** Behavior description says "Builds path as `config_dir/config`" but all directory layouts in install-directories.md show `config.json` extension.

**Resolution needed:** Change to "Builds path as `config_dir/config.json`" for consistency with documented directory layouts.

**GAP-4: Test helper contains function body**

**Location:** paths-module.md, "Testing Strategy" section

**Issue:** Plan shows complete implementation of ik_paths_create_test() test helper, violating "no function bodies" rule from planner skill.

**Consideration:** This is trivial boilerplate that all tests need identically. Showing implementation ensures consistency.

**Resolution needed:**
- Option A: Move to separate test-helpers.md or paths-test-migration.md
- Option B: Add exception for trivial test boilerplate
- Option C: Rewrite as behavioral description

**Recommendation:** Option A - this is already mentioned in paths-test-migration.md, can be expanded there.

**GAP-5: ik_shared_ctx_init() integration incomplete**

**Location:** "Code Cleanup Required" mentions signature change but integration not shown

**Issue:** Plan states "Remove `working_dir` and `ikigai_subdir` parameters from `ik_shared_ctx_init()` signature" but doesn't show:
- Current signature
- New signature
- Call sites that need updating

**Resolution needed:** Add integration point for ik_shared_ctx_init() showing BEFORE/AFTER signatures and affected call sites. Or clarify this is internal function and integration is implementation detail.

**Test Strategy**
- ✓ Unit test scope specified (4 specific test functions)
- ✓ Integration test scope documented (migration affects 160-180 tests)
- ✓ Test tooling specified (environment variables, test helpers)
- ✓ Migration phases defined (3 phases with clear order)
- ✓ Success metrics stated (all 565 tests pass, 100% coverage)

**GAP-6: Inconsistent test helper recommendations**

**Location:** paths-module.md "Testing Strategy" vs paths-test-migration.md "Test Mode Design"

**Issue:** Documents recommend different approaches:
- paths-module.md: Shows `ik_paths_create_test()` that constructs struct directly (bypasses init)
- paths-test-migration.md: "No special test API needed" - use environment variables + ik_paths_init()

**Resolution needed:** Choose one approach consistently:
- Option A: Environment variables only (tests use production code path)
- Option B: Provide ik_paths_create_test() helper (faster setup, but bypasses production code)
- Option C: Provide both, document when to use each

**Recommendation:** Option A - remove ik_paths_create_test() from paths-module.md, use environment variables consistently per paths-test-migration.md. Better test coverage, tests use same code as production.

**No Function Bodies**
- ✓ Function signatures and behavioral descriptions used
- ✓ Struct definitions shown (allowed)
- ✗ Test helper ik_paths_create_test() shows full implementation (GAP-4 above)
- ~ main() integration example shows function body for demonstration

**Minor: main() integration example could be behavioral description**

While main() example clearly shows integration sequence, it could follow "no function bodies" rule more strictly by using numbered steps instead of code. Not blocking since it serves coordination purpose.

**Library Constraints**
- ✓ Uses talloc (approved)
- ✓ Uses POSIX (approved)
- ✓ No new dependencies introduced
- ~ Library constraints stated at plan/README.md level but not repeated in paths-module.md (minor)

**Build Changes**
- ✓ Makefile wrapper script generation identified
- ✓ Wrapper script format specified (4 env vars + exec)
- ✓ Adequate for coordination (HOW is implementation detail)

**Research References**
- ✓ References project/install-directories.md for complete specification
- ✓ No external research needed for infrastructure module

---

## Summary

### Phase 0 Plan Verification Complete

**Documents reviewed:**
- rel-08/README.md
- rel-08/plan/paths-module.md
- rel-08/plan/paths-test-migration.md
- rel-08/user-stories/*.md
- project/install-directories.md

**Gaps found: 6 (5 requiring resolution, 1 minor)**

**Critical gaps:**
1. GAP-1: ERR_INVALID_STATE error code not defined (use ERR_INVALID_ARG instead)
2. GAP-2: Error context allocation not specified (allocate on ctx parameter)
3. GAP-3: Config file path says "config" should be "config.json"
4. GAP-5: ik_shared_ctx_init() signature change incomplete specification
5. GAP-6: Inconsistent test helper recommendations (use env vars only)

**Non-critical:**
- GAP-4: Test helper shows function body (move to test migration doc or remove)
- Minor: Library constraints could be repeated in paths-module.md
- Minor: main() integration could use behavioral description

**Strengths:**
- ✓ Clear wrapper script approach (simple, testable)
- ✓ Comprehensive test migration strategy (160-180 tests classified)
- ✓ Integration points well-specified
- ✓ Memory management and ownership clear
- ✓ Naming conventions followed consistently
- ✓ Test strategy thorough (3 phases, success metrics defined)

**Recommendation:** Address critical gaps before proceeding to task authoring. Plan is otherwise well-structured for Phase 0 implementation.
