# Migration Plan: jansson → yyjson

**Date:** 2025-11-15
**Status:** ✅ COMPLETE - All steps finished, manual testing passed
**Background:** See [docs/jansson_to_yyjson_proposal.md](docs/jansson_to_yyjson_proposal.md)

---

## Documentation Reference

**📚 Project Documentation Index:** [docs/README.md](docs/README.md)

This plan follows strict TDD methodology as defined in [AGENTS.md](AGENTS.md):
- **Red**: Write failing test first (with stub implementation that compiles)
- **Green**: Write minimal code to make test pass
- **Verify**: Run `make fmt && make check && make lint && make coverage && make check-dynamic`

---

## Current State

✅ **jansson fully removed from ikigai codebase!**
- All source code migrated to yyjson
- All build systems updated (Makefile, Debian, Fedora, Arch packaging)
- All tests passing with 100% coverage
- Zero jansson dependencies in our code

**Note on Arch Linux:** jansson appears as a transitive dependency of `base-devel` (Arch's build tools meta-package). This is outside our control - we don't use or link against it.

---

## Migration Steps

### Step 1: Vendor yyjson and Create talloc Allocator ✅ COMPLETE

**Goal:** Add yyjson to codebase with talloc integration, verify build

**Tasks:**
1. ✅ Download yyjson.c and yyjson.h from https://github.com/ibireme/yyjson
2. ✅ Place in `src/vendor/yyjson/`
3. ✅ Create `src/json_allocator.{c,h}` with talloc wrapper (see proposal:103-136)
4. ✅ Update `Makefile` to compile yyjson.c (CLIENT_SOURCES and MODULE_SOURCES)
5. ✅ Verify clean build: `make clean && make`
6. ✅ Exclude vendor code from coverage reporting

**TDD Note:** No tests yet - just infrastructure setup. Both jansson and yyjson will coexist temporarily.

**Success Criteria:**
- ✅ `make` compiles without errors
- ✅ yyjson vendored files present
- ✅ json_allocator module compiles
- ✅ **No functional changes yet**

---

### Step 2: Migrate src/wrapper.{c,h} (TDD) ⏭️ SKIPPED

**Goal:** Replace jansson wrapper functions with yyjson equivalents

**Decision:** Skipped - jansson wrappers only used in test mocking, not production code. Will create yyjson wrappers as needed for test coverage in Step 3.1.

**Current wrapper.h functions to migrate:**
- `ik_json_object_set_new_wrapper()`
- `ik_json_loads_wrapper()`
- Any other json_* wrappers

**TDD Process:**

**2.1 Read and understand current wrapper usage**
```bash
# Find all calls to wrapper functions
grep -r "ik_json_" src/ tests/
```

**2.2 For EACH wrapper function:**

**RED Phase:**
- Write test that calls new yyjson-based function (will fail)
- Add function declaration to `wrapper.h`
- Add stub implementation in `wrapper.c` that compiles but returns error/NULL
- Verify test FAILS with assertion (not compilation error)

**GREEN Phase:**
- Implement yyjson version using talloc allocator
- Run test until it passes
- STOP when test passes - no premature optimization

**VERIFY Phase:**
```bash
make fmt
make check        # All tests pass
make lint         # Complexity checks pass
make coverage     # 100% coverage maintained
make check-dynamic # Sanitizers clean
```

**Success Criteria:**
- All wrapper functions migrated to yyjson
- All tests pass
- 100% test coverage maintained
- Both jansson and yyjson still in build (config.c still uses jansson)

---

### Step 3: Migrate src/config.c (TDD) 🔄 PARTIALLY COMPLETE

**Goal:** Replace jansson usage in config.c with yyjson

**Status:** Migration complete, all tests passing. Coverage: 99.9% lines, 100% functions, 99.0% branches. Need Step 3.1 to reach 100%.

**Current config.c jansson usage:**
- `json_loads()` - Parse JSON config file
- `json_object_get()` - Extract config values
- `json_string_value()`, `json_integer_value()` - Type conversions
- `json_decref()` - Manual cleanup (will be eliminated!)

**TDD Process:**

**3.1 Identify all config parsing functions**
```bash
grep "json_" src/config.c
```

**3.2 For EACH function that uses jansson:**

**RED Phase:**
- **CRITICAL:** Tests already exist for config.c
- Modify implementation to use yyjson (tests will fail)
- Use `yyjson_read_opts()` with talloc allocator
- Verify tests FAIL (wrong results, not compilation errors)

**GREEN Phase:**
- Complete the yyjson migration for that function
- Remove `json_decref()` calls (talloc handles cleanup!)
- Run tests until they pass
- STOP when tests pass

**VERIFY Phase:**
```bash
make fmt
make check
make lint
make coverage     # Must maintain 100%
make check-dynamic
```

**3.3 Remove destructor patterns**
- Search for any `json_decref()` calls - should be ZERO
- Remove destructor functions if they exist
- Verify with: `grep -n "json_decref" src/config.c` (should be empty)

**Success Criteria:**
- ✅ config.c fully migrated to yyjson
- ✅ All config tests pass (100% of test cases)
- ✅ No more `json_decref()` calls
- ✅ 100% coverage maintained (100.0% lines, functions, branches)
- ✅ Memory leaks clean (valgrind/sanitizers)

---

### Step 3.1: Achieve 100% Coverage for config.c ✅ COMPLETE

**Goal:** Add yyjson wrappers to achieve 100% line and branch coverage

**What We Did:**

1. ✅ Created comprehensive yyjson wrappers in `src/wrapper.{c,h}`:
   - `ik_yyjson_read_file_wrapper()` - mock file read errors
   - `ik_yyjson_mut_write_file_wrapper()` - mock file write errors
   - `ik_yyjson_doc_get_root_wrapper()` - mock NULL root
   - `ik_yyjson_obj_get_wrapper()` - mock missing keys
   - `ik_yyjson_get_sint_wrapper()` - wrapper for int extraction
   - `ik_yyjson_get_str_wrapper()` - wrapper for string extraction
   - All follow MOCKABLE pattern (weak symbols in debug, inline in release)

2. ✅ Updated config.c to use all wrappers instead of direct yyjson calls

3. ✅ Added comprehensive tests in `tests/integration/config_integration_test.c`:
   - `test_config_write_failure` - yyjson_mut_write_file error path
   - `test_config_read_failure` - yyjson_read_file error path
   - `test_config_invalid_json_root` - JSON root is array not object
   - `test_config_doc_get_root_null` - doc_get_root returns NULL

4. ✅ Removed invalid LCOV exclusions (vendor function internal branches)
   - Wrapped and tested all vendor function error paths
   - Kept only valid exclusions (asserts and OOM PANICs)

**Key Lesson Learned:**
If we rely on vendor function checks, we MUST wrap them and test both paths. Making error paths "logically impossible" through validation is a design flaw - it prevents testing the very error handling we depend on.

**Final Coverage:**
- Overall: 100.0% lines (1816/1816), 100.0% functions (135/135), 100.0% branches (608/608)
- config.c: 100% lines, 100% branches
- LCOV exclusions: 308/340 (only asserts and OOM PANICs)
- All quality gates passing

**Success Criteria:**
- ✅ config.c: 100% lines, 100% branches
- ✅ Overall: 100% lines, 100% functions, 100% branches
- ✅ All tests still passing (11 config tests total)
- ✅ Wrappers ready for future LLM module use
- ✅ All LCOV exclusions justified and documented

---

### Step 4: Migrate tests/test_utils.{c,h} (TDD) ✅ COMPLETE

**Goal:** Replace jansson usage in test utilities

**Status:** Complete - jansson wrapper functions were dead code (never used), removed cleanly

**What We Did:**

1. ✅ Inventoried jansson usage in test_utils.{c,h}:
   - Found 4 jansson wrapper functions (ik_json_object_wrapper, ik_json_dumps_wrapper, ik_json_is_object_wrapper, ik_json_is_string_wrapper)
   - Verified they were never actually called anywhere in the codebase (dead code from removed protocol module)

2. ✅ Removed dead jansson code:
   - Removed jansson wrapper declarations from tests/test_utils.h
   - Removed jansson wrapper implementations from tests/test_utils.c
   - Removed jansson wrapper declarations from src/wrapper.h
   - Removed jansson wrapper implementations from src/wrapper.c
   - Removed #include <jansson.h> from all files

3. ✅ Verified build and tests:
   - All tests pass (100%)
   - All quality gates pass (fmt, check, lint, coverage)
   - 100% coverage maintained

**Success Criteria:**
- ✅ All jansson code removed from test utilities
- ✅ All jansson code removed from wrapper.{c,h}
- ✅ All tests pass
- ✅ No jansson usage in test code or production code

---

### Step 5: Remove jansson Completely ✅ COMPLETE

**Goal:** Eliminate jansson from codebase

**Status:** Complete - jansson fully removed from all build systems and packaging

**What We Did:**

1. ✅ Verified zero jansson usage in source code:
   - No jansson.h includes in src/ or tests/ (except historical comment in config.c)
   - No jansson API calls anywhere
   - Only yyjson used for JSON parsing

2. ✅ Removed from build system:
   - Removed `-ljansson` from CLIENT_LIBS in Makefile
   - Removed `-ljansson` from all test link commands (unit, integration, performance)
   - Removed `libjansson-dev` from PACKAGES list in Makefile

3. ✅ Removed from Debian packaging:
   - Removed `libjansson-dev` from distros/debian/Dockerfile
   - Removed `libjansson-dev` from distros/debian/packaging/control

4. ✅ Removed from Fedora packaging:
   - Removed `jansson-devel` from distros/fedora/Dockerfile
   - Removed `-ljansson` from SERVER_LIBS in distros/fedora/Dockerfile
   - Removed `jansson-devel` from BuildRequires in distros/fedora/packaging/ikigai.spec
   - Removed `jansson` from Requires in distros/fedora/packaging/ikigai.spec

5. ✅ Removed from Arch packaging:
   - Removed `jansson` from distros/arch/Dockerfile (both pacman install and PACKAGES)
   - Removed `-ljansson` from SERVER_LIBS in distros/arch/Dockerfile
   - Removed `jansson` from depends in distros/arch/packaging/PKGBUILD

6. ✅ Verified clean build:
   - make clean && make fmt && make check && make lint && make coverage - ALL PASS
   - 100% coverage maintained (1816/1816 lines, 135/135 functions, 608/608 branches)

**Success Criteria:**
- ✅ Zero references to jansson in source code (only historical comment remains)
- ✅ Build succeeds without jansson dependency
- ✅ All quality gates pass
- ✅ All packaging files updated (Debian, Fedora, Arch)

---

### Step 6: Update Documentation

**Goal:** Replace all jansson references with yyjson

**Files to update (found via grep):**
- [x] `docs/README.md` - Update LLM Integration section
- [x] `docs/architecture.md` - Update dependency list
- [x] `docs/memory.md` - Update JSON memory management examples
- [x] `docs/build-system.md` - Update dependency list
- [x] `docs/jansson_to_yyjson_proposal.md` - Mark as COMPLETED
- [x] `docs/repl/phase-5-plan.md` - Update if references jansson
- [x] `docs/repl/README.md` - Update if references jansson
- [x] `docs/memory_usage_analysis.md` - Update JSON examples
- [x] `docs/decisions/link-seams-mocking.md` - Update if references jansson

**For EACH document:**
1. Read current content
2. Replace jansson references with yyjson
3. Update code examples to use yyjson API
4. Add note about talloc integration benefits

**Success Criteria:**
- `grep -r jansson docs/` returns only historical references in proposal
- All examples use yyjson API
- Documentation reflects single memory model (talloc only)

---

### Step 7: Final Verification ✅ COMPLETE

**Goal:** Human verification before considering migration complete

**7.1 Code Quality Gates** ✅ (automated)
```bash
make clean         # ✅ PASS
make fmt           # ✅ PASS
make check         # ✅ PASS - ALL tests pass
make lint          # ✅ PASS - ALL complexity checks pass
make coverage      # ✅ PASS - 100.0% lines (1816/1816), functions (135/135), branches (608/608)
make check-dynamic # ✅ PASS - ASan, UBSan, TSan, Valgrind, Helgrind clean
```

**7.2 Multi-Distro Validation** (if desired)
```bash
# Test on Debian, Fedora, Arch
cd distros/debian && docker build .
cd distros/fedora && docker build .
cd distros/arch && docker build .
```

**7.3 Human Verification Checklist** ✅

- [x] Review git diff - no unintended changes
- [x] Verify yyjson vendored correctly (MIT license file present)
- [x] Spot-check config.c - no json_decref() calls remain
- [x] Confirm cleaner error handling (fewer cleanup branches)
- [x] Review talloc allocator implementation for correctness
- [x] Verify documentation updates are accurate
- [x] Test actual config file loading manually
- [x] Confirm memory usage is reasonable (talloc-report if needed)
- [x] Code review: any security concerns with new parser?

**7.4 Create Commit**

Per [AGENTS.md](AGENTS.md):195-197:
- **NO** "Co-Authored-By: Claude" attribution
- **NO** "🤖 Generated with Claude Code" footer
- Use clear, concise commit message

Example:
```
Migrate from jansson to yyjson for better talloc integration

- Vendor yyjson (MIT license) in src/vendor/yyjson/
- Create talloc allocator wrapper for unified memory management
- Migrate config.c, wrapper.{c,h}, test_utils.{c,h}
- Remove all jansson dependencies and destructor patterns
- Update documentation to reflect yyjson usage
- Maintain 100% test coverage throughout migration
```

---

## Key Benefits Achieved ✅

✅ **Single memory model** - talloc only, no reference counting
✅ **Automatic cleanup** - no more `json_decref()` in error paths
✅ **Simpler code** - no destructor boilerplate needed
✅ **Better performance** - 3× faster parsing for future LLM streaming
✅ **Fewer bugs** - impossible to forget reference counting
✅ **No jansson dependency** - one less external dependency to manage
✅ **Vendored yyjson** - full control over JSON parsing with MIT license

---

## Next Steps After Migration

See [docs/README.md](docs/README.md):79-91 for next feature:
- **OpenAI API Integration** (now with fast yyjson parsing!)

---

## Rollback Plan

If critical issues discovered during Step 7:
1. `git reset --hard` to before migration
2. Document specific issues found
3. Revise this plan to address issues
4. Retry migration

---

**Notes:**
- Each step must complete with ALL quality gates passing before proceeding
- Maintain 100% test coverage throughout (no regressions)
- TDD discipline: RED → GREEN → VERIFY for every change
- Ask human if ANY uncertainty arises during migration
