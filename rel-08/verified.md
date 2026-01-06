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
