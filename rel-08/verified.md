# Verified Items

This file tracks plan verification to avoid duplicate work.

## Format

Each verified item includes:
- Item name/path
- What was verified
- Date verified
- Any issues found and resolved

---

## 2026-01-04: Initial Verification

### paths-module.md - GAPS FOUND (RESOLVED 2026-01-05)

**Original gaps:**

1. **Function implementation code in plan (CRITICAL)** - Detection algorithm contained if/else implementation code
2. **Hardcoded path checks** - Only recognized `/usr/bin` and `/usr/local/bin`
3. **PREFIX handling inconsistencies** - Couldn't support `/opt/ikigai` or `$HOME/.local` properly

**Resolution:**

Completely redesigned approach using wrapper script pattern:
- Makefile generates wrapper script at install time
- Wrapper sets IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR environment variables
- paths.c simply reads environment variables (no complex detection logic)
- Supports all 4 standard prefixes: `/usr`, `/usr/local`, `/opt/ikigai`, `$HOME/.local`
- Works with .envrc for development
- Works with test environment variable overrides
- Uninstall target mirrors install with same PREFIX

**Status:** VERIFIED - Plan now describes behavior without implementation details and supports all expected install prefixes

### architecture.md - GAPS FOUND

**What was verified:**
- Naming conventions (PASS - all follow ik_* pattern)
- Return value conventions (PASS - res_t for failable ops, void/pointers for others)
- Memory management (PASS - talloc ownership described)
- Integration points (PASS - shows where new code connects)
- Research references (PASS - references research docs)

**GAPS:**

1. **Function implementation code in plan (CRITICAL)**
   - Lines 90-108: `ik_tool_discovery_run()` implementation contains while loop, if/else, select() call
   - Lines 230-235: Code snippet showing yyjson_mut_obj_add_val() calls with if statement

   **Violation:** Same as paths-module.md - no function bodies in plan documents.

   **Fix needed:**
   - Lines 90-108: Replace with behavioral description: "ik_tool_discovery_run() starts async discovery, blocks in select() loop until complete, finalizes and returns result"
   - Lines 230-235: Replace code with description: "Request building checks if tools array is non-empty, adds to request JSON if present, omits if empty"

### removal-specification.md - VERIFIED

**What was verified:**
- Naming conventions (N/A - removal spec)
- Contains exact code snippets (ACCEPTABLE per planner skill exception)
- Complete file lists for deletion
- Specific line numbers for modifications

**GAPS:** None

**Note:** removal-specification.md is a patch specification and is allowed to show exact code to find/replace.

### integration-specification.md - VERIFIED

**What was verified:**
- Naming conventions (PASS - consistent with project standards)
- Return value conventions (PASS - behavioral descriptions, no implementation)
- Memory management (PASS - describes ownership, thread safety)
- Integration points (PASS - complete call chains, struct changes)
- Function signatures (PASS - shows signatures, not bodies)

**GAPS:** None

**Strength:** Excellent specification of exact integration points with behavioral descriptions instead of implementation code.

### tool-discovery-execution.md - VERIFIED

**What was verified:**
- Behavioral descriptions (PASS)
- Code examples (ACCEPTABLE - showing format/protocol, not implementation)
- Discovery protocol specification (PASS)

**GAPS:** None

### test-specification.md - VERIFIED

**What was verified:**
- Test structure guidance (PASS)
- Manual verification steps (PASS)
- TDD workflow included (PASS)

**GAPS:** None

---

## Alignment Verification

### README → User Stories

**README features (from cdd/README.md):**
1. Add custom tools - drop executable in ~/.ikigai/tools/, run /refresh
2. Zero token tax - custom tools run at native efficiency
3. Three-tier discovery (system/user/project)
4. /tool and /refresh commands
5. Tool execution with JSON protocol
6. Response wrapper

**User story coverage:**
- ✓ add-custom-tool.md - covers feature #1
- ✓ startup-experience.md - covers async discovery (Phase 6)
- ✓ list-tools.md - covers /tool command (feature #4)
- ✓ inspect-tool.md - covers /tool NAME command (feature #4)
- ✓ tool-missing-credentials.md - covers self-service setup
- ✓ tool-failure.md - covers response wrapper (feature #6)

**GAP FOUND:** No user story for /refresh command explicitly, but it's shown in add-custom-tool.md step 4.

**ASSESSMENT:** Alignment is GOOD. All major features have user story coverage.

### User Stories → Plan

**User story: add-custom-tool.md expectations:**
1. Drop tool in ~/.ikigai/tools/
2. Run /refresh
3. Tool appears in registry
4. LLM can call it

**Plan coverage:**
- ✓ architecture.md describes three-tier discovery (system/user/project)
- ✓ tool-discovery-execution.md describes scanning all three directories
- ✓ architecture.md describes /refresh command
- ✓ integration-specification.md describes tool execution flow

**User story: startup-experience.md (Phase 6):**
- Terminal appears immediately
- Tools load in background
- User can type while loading

**Plan coverage:**
- ✓ architecture.md Phase 6 describes async optimization
- ✓ integration-specification.md Phase 6 shows event loop integration

**ASSESSMENT:** User stories align with plan. Plan implements what stories demonstrate.

### README → Plan

**README promises:**
- "Drop an executable in ~/.ikigai/tools/, run /refresh, done"
- "Zero token tax"
- "Tools manage their own credentials"
- "Override precedence: Project > User > System"

**Plan delivers:**
- ✓ tool-discovery-execution.md: three-tier discovery with override precedence
- ✓ architecture.md: /refresh command
- ✓ tool-discovery-execution.md: credential-independent discovery
- ✓ architecture.md: external tool execution (zero token overhead vs bash)

**ASSESSMENT:** Plan implements README promises.

---


## 2026-01-05: Phase 0 Path Resolution Verification

### Scope
Verifying phase 0 installation path plan documents:
- plan/paths-module.md
- plan/paths-test-migration.md
- project/install-directories.md (reference documentation, different rules)

### Alignment Verification - PASS

**README → Plan:**
- ✓ README promises: "Install directory detection (FIRST STEP)"
- ✓ Plan delivers: Wrapper script approach with environment variables
- ✓ README says: "Detects install type (development/user/system)"
- ✓ Plan delivers: Support for /usr, /usr/local, /opt/ikigai, $HOME/.local
- ✓ README says: "Provides paths to all subsystems via dependency injection"
- ✓ Plan delivers: ik_paths_t instance passed to REPL, tools, config

**README → User Stories:**
- Phase 0 is infrastructure, no direct user-facing behavior
- User stories reference three-tier discovery (system/user/project)
- ✓ Plan provides paths for all three tiers

**Assessment:** Alignment is STRONG. Plan implements what README promises.

### Convention Checks

**1. Naming Conventions - PASS**
- ✓ All symbols follow ik_paths_* pattern
- ✓ Type: ik_paths_t (opaque struct)
- ✓ Functions: ik_paths_init(), ik_paths_get_*()
- ✓ Module name: "paths" (clear, single word)

**2. Return Value Conventions - PASS**
- ✓ ik_paths_init() returns res_t with OK()/ERR()
- ✓ Getters return const char* (cannot fail, no res_t needed)
- ✓ Error cases documented: ERR_INVALID_STATE for missing env vars

**3. Memory Management - PASS**
- ✓ Talloc ownership specified: paths instance owned by parent
- ✓ All path strings owned by ik_paths_t instance
- ✓ Parent-child relationship clear
- ✓ No manual cleanup needed (freed with root_ctx)

**4. Integration Points - PASS**
- ✓ Call sites specified: src/client.c (main initialization)
- ✓ Struct changes specified: ik_repl_ctx_t gains ik_paths_t *paths field
- ✓ Signature changes documented:
  - ik_shared_ctx_init() loses working_dir/ikigai_subdir, gains ik_paths_t*
  - ik_cfg_load() loses path parameter, gains ik_paths_t*
- ✓ Tool discovery receives directory paths from ik_paths_t

**5. Research References - PASS**
- ✓ References project/install-directories.md (complete specification)
- ✓ No external research needed (standard POSIX/FHS patterns)

**6. Build Changes - PASS**
- ✓ Makefile changes specified: wrapper script generation
- ✓ Install/uninstall targets need update
- ✓ Development mode specified: .envrc approach
- ✓ Environment variables documented: IKIGAI_BIN_DIR, etc.

**7. Database Migrations - N/A**
- No database changes in phase 0

**8. Test Strategy - PASS**
- ✓ Test approach defined: ik_paths_create_test() helper
- ✓ Test migration strategy detailed in paths-test-migration.md
- ✓ Impact scope documented: 160+ tests affected
- ✓ Helper infrastructure specified: tests/helpers/test_contexts.c
- ✓ Coverage expectations clear: 100% requirement maintained

**9. Library Constraints - PASS**
- ✓ talloc for memory management
- ✓ POSIX for environment variables (getenv)
- ✓ No new dependencies introduced

**10. Coordination Completeness - PASS**
- ✓ Function signatures complete (all parameters, return types)
- ✓ Struct definition complete (all fields, types)
- ✓ Integration contracts clear (DI pattern)
- ✓ Error handling approach defined (res_t with specific error codes)


### CRITICAL GAP: Function Implementation Code in Plan Documents

**Violation:** Plan documents contain function bodies with full implementation code. 
Per planner skill: "Plan should NOT include function bodies or implementation code."

**Gap 1: plan/paths-module.md - Multiple Function Bodies**

Lines 149-174: Full implementation of ik_paths_init()
```
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out) {
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    // Read from environment (set by wrapper script)
    const char *bin = getenv("IKIGAI_BIN_DIR");
    ...
    return OK();
}
```

Lines 245-255: Full implementation of ik_cfg_load()
```
res_t ik_cfg_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out_cfg) {
    char *config_path = talloc_asprintf(ctx, "%s/config", ...);
    if (file_exists(config_path)) {
        return load_config_file(ctx, config_path, out_cfg);
    }
    ...
}
```

Lines 336-344: Full implementation of ik_paths_create_test()
```
ik_paths_t *ik_paths_create_test(TALLOC_CTX *ctx) {
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    paths->bin_dir = talloc_strdup(paths, "tests/bin");
    ...
    return paths;
}
```

**Gap 2: plan/paths-test-migration.md - Function Bodies**

Lines 417-445: Full implementation of ik_cfg_load() with config search logic
```
res_t ik_cfg_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_cfg_t **out)
{
    // Try project config first
    char *project_config = "./.ikigai/config";
    if (file_exists(project_config)) {
        return ik_cfg_load_file(ctx, project_config, out);
    }
    ...
}
```

Lines 280-288: Full implementation of test helper
```
res_t test_shared_ctx_create(TALLOC_CTX *ctx, ik_shared_ctx_t **out)
{
    ik_paths_t *paths = ik_paths_create_for_test(ctx, test_prefix);
    ...
}
```

**Required Fix:**

Replace all function bodies with behavioral descriptions:

**Example - Instead of:**
```c
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out) {
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    const char *bin = getenv("IKIGAI_BIN_DIR");
    ...
}
```

**Write:**
```c
// Function signature
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out);

// Behavior
Reads IKIGAI_BIN_DIR, IKIGAI_CONFIG_DIR, IKIGAI_DATA_DIR, IKIGAI_LIBEXEC_DIR 
from environment. Allocates ik_paths_t on ctx, copies paths to struct fields.
Expands tilde in user tools path. Returns ERR_INVALID_STATE if any env var missing.
Returns OK() with paths instance on success.
```

**Note on project/install-directories.md:**
This file is in project/ (reference documentation), not cdd/plan/ (plan documents).
Reference docs can include implementation examples. No changes needed there.

**Severity:** CRITICAL - Plan documents must describe WHAT, not HOW.

**Next Action:** Fix both plan documents to remove implementation code and replace 
with behavioral descriptions.


---

### Summary: Phase 0 Path Resolution Verification

**Documents Verified:**
- plan/paths-module.md
- plan/paths-test-migration.md
- project/install-directories.md (reference doc, different rules)

**Alignment:** PASS ✓
- Plan implements README promises
- Coordinates with tool discovery (Phase 1-6)
- Provides foundation for dependency injection

**Conventions:** MOSTLY PASS
- ✓ Naming conventions
- ✓ Return value conventions
- ✓ Memory management
- ✓ Integration points
- ✓ Test strategy
- ✓ Library constraints
- ✗ NO FUNCTION BODIES - CRITICAL GAP FOUND

**Status:** VERIFIED WITH CRITICAL GAPS

**Required Action:** Remove implementation code from plan documents. Replace with 
behavioral descriptions as shown in examples above.

**Once fixed:** Phase 0 plan will be complete and ready for task authoring.


---

## 2026-01-05: Phase 0 Plan Updates - Single-Layer Config + Test Simplification

### Changes Made

**1. Single-Layer Config System**
- Removed cascading config search (project > user > system)
- Config now loads from single location: IKIGAI_CONFIG_DIR/config
- Simplified plan/paths-module.md section 4 (Config Loading)
- Simplified plan/paths-test-migration.md (Config Module Integration)

**Rationale:** Per user request, use only install location config. No cascading.

**2. Removed Function Implementation Code**
Replaced all function bodies with behavioral descriptions:
- plan/paths-module.md: ik_paths_init(), ik_cfg_load(), test helpers
- plan/paths-test-migration.md: ik_cfg_load(), test helpers

**3. Simplified Test Strategy**
Changed from struct construction to environment variables:
- Tests set IKIGAI_*_DIR environment variables
- Tests call ik_paths_init() (same as production)
- No special test APIs (ik_paths_create_for_test, etc.)
- No struct definition duplication in tests

**Test helpers:**
- `test_paths_setup_env()`: Sets standard test env vars
- `test_paths_cleanup_env()`: Unsets env vars
- Custom tests: setenv() directly

**Benefits:**
- Tests use production code path exactly
- Simpler (no struct construction)
- Matches wrapper script approach

### Status: Phase 0 Plan VERIFIED

**Alignment:** PASS ✓
**Conventions:** PASS ✓ (function bodies removed)
**Config System:** Single-layer only ✓
**Test Strategy:** Simplified to env vars ✓

Phase 0 plan is complete and ready for task authoring.

