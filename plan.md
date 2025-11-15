# Migration Plan: jansson → yyjson

**Date:** 2025-11-15
**Status:** READY TO EXECUTE
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

jansson is used in 4 files:
- `src/config.c` - Config file parsing (primary usage)
- `src/wrapper.{c,h}` - JSON wrapper functions
- `tests/test_utils.{c,h}` - Test utilities for JSON

Protocol module was removed in Phase 2.5, so scope is smaller than original proposal.

---

## Migration Steps

### Step 1: Vendor yyjson and Create talloc Allocator

**Goal:** Add yyjson to codebase with talloc integration, verify build

**Tasks:**
1. Download yyjson.c and yyjson.h from https://github.com/ibireme/yyjson
2. Place in `src/vendor/yyjson/`
3. Create `src/json_allocator.{c,h}` with talloc wrapper (see proposal:103-136)
4. Update `Makefile` to compile yyjson.c
5. Verify clean build: `make clean && make`

**TDD Note:** No tests yet - just infrastructure setup. Both jansson and yyjson will coexist temporarily.

**Success Criteria:**
- `make` compiles without errors
- yyjson vendored files present
- json_allocator module compiles
- **No functional changes yet**

---

### Step 2: Migrate src/wrapper.{c,h} (TDD)

**Goal:** Replace jansson wrapper functions with yyjson equivalents

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

### Step 3: Migrate src/config.c (TDD)

**Goal:** Replace jansson usage in config.c with yyjson

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
- config.c fully migrated to yyjson
- All config tests pass
- No more `json_decref()` calls
- 100% coverage maintained
- Memory leaks clean (valgrind/sanitizers)

---

### Step 4: Migrate tests/test_utils.{c,h} (TDD)

**Goal:** Replace jansson usage in test utilities

**TDD Process:**

**4.1 Inventory test utilities**
- Find all jansson usage in test_utils.{c,h}
- Identify which tests use these utilities

**4.2 For EACH test utility function:**

**RED Phase:**
- Convert function to use yyjson
- Tests using this utility will fail
- Verify tests FAIL (not compilation errors)

**GREEN Phase:**
- Complete yyjson migration
- Update test code as needed
- Run until tests pass

**VERIFY Phase:**
```bash
make check
make check-dynamic
```

**Success Criteria:**
- All test utilities migrated
- All tests pass
- No jansson usage in test code

---

### Step 5: Remove jansson Completely

**Goal:** Eliminate jansson from codebase

**5.1 Verify zero jansson usage**
```bash
# Should return NOTHING:
grep -r "jansson\.h" src/
grep -r "jansson\.h" tests/
grep -r "json_" src/ | grep -v yyjson
```

**5.2 Remove from build system**
- Remove jansson from `Makefile`
- Remove jansson from `distros/*/Dockerfile`
- Remove jansson from `distros/*/packaging/*` (PKGBUILD, .spec, control)

**5.3 Test clean build on multiple distros**
```bash
make clean
make           # Should build without jansson
make check     # All tests pass
```

**Success Criteria:**
- Zero references to jansson in source code
- Build succeeds without jansson installed
- All quality gates pass

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

### Step 7: Final Verification (MANUAL - HUMAN REQUIRED)

**Goal:** Human verification before considering migration complete

**7.1 Code Quality Gates** (automated)
```bash
make clean
make fmt
make check         # ALL tests pass
make lint          # ALL complexity checks pass
make coverage      # 100.0% lines, functions, branches
make check-dynamic # ASan, UBSan, TSan clean
```

**7.2 Multi-Distro Validation** (if desired)
```bash
# Test on Debian, Fedora, Arch
cd distros/debian && docker build .
cd distros/fedora && docker build .
cd distros/arch && docker build .
```

**7.3 Human Verification Checklist**

- [ ] Review git diff - no unintended changes
- [ ] Verify yyjson vendored correctly (MIT license file present)
- [ ] Spot-check config.c - no json_decref() calls remain
- [ ] Confirm cleaner error handling (fewer cleanup branches)
- [ ] Review talloc allocator implementation for correctness
- [ ] Verify documentation updates are accurate
- [ ] Test actual config file loading manually
- [ ] Confirm memory usage is reasonable (talloc-report if needed)
- [ ] Code review: any security concerns with new parser?

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

## Key Benefits Achieved

✅ **Single memory model** - talloc only, no reference counting
✅ **Automatic cleanup** - no more `json_decref()` in error paths
✅ **Simpler code** - no destructor boilerplate needed
✅ **Better performance** - 3× faster parsing for future LLM streaming
✅ **Fewer bugs** - impossible to forget reference counting

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
