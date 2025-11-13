# Proposal: Migrate from jansson to yyjson

**Date:** 2025-11-12
**Status:** APPROVED - 2025-11-13
**Decision:** Migrate to yyjson before LLM Integration phase
**Rationale:** Better talloc integration (primary) + 3× faster parsing (secondary)

---

## Executive Summary

This proposal evaluates migrating ikigai's JSON library from **jansson** (reference counting) to **yyjson** (custom allocator support) to eliminate the lifecycle mismatch between jansson's reference counting and talloc's hierarchical memory management.

**Key Findings:**
- ✅ **yyjson integrates cleanly with talloc** via custom allocator hooks
- ✅ **3x faster parsing** than jansson (relevant for streaming LLM responses)
- ✅ **Eliminates destructor pattern** - all memory in talloc hierarchy
- ⚠️ **Requires rewriting all JSON code** (~400 lines)
- ⚠️ **Less battle-tested** than jansson (newer library, est. 2020)
- ⚠️ **Migration effort:** Estimated 6-8 hours + testing

**Decision (2025-11-13):** **MIGRATE TO YYJSON** - Better talloc integration eliminates destructor pattern and reference counting complexity. Migration to occur before LLM Integration phase.

---

## Table of Contents

1. [Current State: jansson](#current-state-jansson)
2. [Proposed State: yyjson](#proposed-state-yyjson)
3. [Benefits of Migration](#benefits-of-migration)
4. [Risks and Costs](#risks-and-costs)
5. [Technical Comparison](#technical-comparison)
6. [Migration Strategy](#migration-strategy)
7. [Code Changes Required](#code-changes-required)
8. [Research Required Before Decision](#research-required-before-decision)
9. [Decision Criteria](#decision-criteria)
10. [Recommendation](#recommendation)

---

## Current State: jansson

### Overview

**Library:** jansson v2.14.1 (2021, actively maintained)
**Memory Model:** Reference counting (`json_incref()` / `json_decref()`)
**Integration:** talloc destructors call `json_decref()` to bridge memory models

### Current Usage

**Files Using jansson:**
- `src/protocol.c` - Protocol message parsing and serialization (primary usage)
- `src/protocol.h` - Message structure with `json_t *payload` field
- `src/config.c` - Configuration file parsing (simple usage, no storage)
- `src/wrapper.c` - Wrapper for `json_is_object()` (mockable for tests)
- `tests/unit/protocol/*.c` - Extensive test coverage

**Lines of Code:** ~400 lines directly using jansson API

### Current Integration Pattern

```c
typedef struct {
    char *sess_id;
    char *type;
    json_t *payload;  // ← jansson-managed with reference counting
} ik_protocol_msg_t;

// Destructor bridges jansson → talloc
static int ik_protocol_msg_destructor(ik_protocol_msg_t *msg)
{
    assert(msg != NULL);
    if (msg->payload) {
        json_decref(msg->payload);  // Decrement reference count
        msg->payload = NULL;
    }
    return 0;
}

// Set destructor when allocating message
ik_protocol_msg_t *msg = talloc_zero(ctx, ik_protocol_msg_t);
talloc_set_destructor(msg, ik_protocol_msg_destructor);
```

### Known Issues

1. **Critical Bug:** `ik_protocol_msg_create_assistant_resp()` doesn't `json_incref()` when taking ownership (identified in memory analysis)
2. **Lifecycle Mismatch:** Two memory management systems (talloc hierarchy + jansson refcounting)
3. **Destructor Overhead:** Every message struct requires destructor boilerplate
4. **Error Path Complexity:** 11 manual `json_decref()` calls in `ik_protocol_msg_parse()`
5. **Test Complexity:** Manual `json_decref()` required in error path tests

### What Works Well

- ✅ **Mature and stable** - jansson is battle-tested (2009-present)
- ✅ **Excellent documentation** - Clear API reference and examples
- ✅ **Type-safe API** - Compile-time type checking for JSON operations
- ✅ **Active maintenance** - Regular releases, responsive maintainers
- ✅ **Wide adoption** - Used in many production systems
- ✅ **Already working** - Current implementation is functionally correct (after bug fix)

---

## Proposed State: yyjson

### Overview

**Library:** yyjson v0.10.0 (2020-present, actively maintained)
**Memory Model:** Custom allocator support, immutable documents
**Integration:** Direct talloc allocation, no destructors needed

### Key Features

1. **Custom Allocator Support**
   ```c
   yyjson_alc allocator = {
       .malloc = talloc_malloc_wrapper,
       .realloc = talloc_realloc_wrapper,
       .free = talloc_free
   };
   yyjson_doc *doc = yyjson_read_opts(json_str, len, 0, &allocator, NULL);
   ```

2. **High Performance**
   - Benchmarked as fastest JSON library in C
   - ~3x faster parsing than jansson
   - ~2x faster serialization than jansson
   - SIMD optimizations on supported platforms

3. **Modern API Design**
   - Clean separation: immutable read, mutable write
   - Zero-copy parsing option (for read-only use cases)
   - Iterator support for arrays/objects
   - Comprehensive error reporting

4. **Memory Efficiency**
   - Pooled allocations reduce fragmentation
   - Compact representation (less memory per node)
   - In-place string storage (no separate string allocations)

### Proposed Integration Pattern

```c
typedef struct {
    char *sess_id;
    char *type;
    yyjson_doc *payload_doc;  // ← yyjson document (talloc-managed)
    yyjson_val *payload;      // ← Root value (borrowed from doc)
} ik_protocol_msg_t;

// NO DESTRUCTOR NEEDED - talloc owns everything
// yyjson_doc allocated via talloc, freed when msg freed

// Parse with custom allocator
res_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str) {
    yyjson_alc alc = make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(json_str, strlen(json_str), 0, &alc, NULL);

    if (!doc) {
        return ERR(ctx, PARSE, "JSON parse error");
    }

    // All memory allocated via talloc - no destructor needed
    ik_protocol_msg_t *msg = talloc_zero(ctx, ik_protocol_msg_t);
    msg->payload_doc = doc;
    msg->payload = yyjson_doc_get_root(doc);

    return OK(msg);
}
```

---

## Benefits of Migration

### 1. Eliminate Lifecycle Mismatch ✅ HIGH VALUE

**Current Problem:**
- jansson uses reference counting (malloc/free)
- talloc uses hierarchical ownership
- Destructor pattern bridges the gap (boilerplate)

**After Migration:**
- All JSON memory allocated via talloc
- Single ownership model throughout codebase
- No destructors needed for JSON cleanup
- Simpler mental model for developers

**Impact:** Cleaner architecture, fewer bugs related to ownership confusion

### 2. Performance Improvement ✅ MEDIUM-HIGH VALUE

**Benchmarks (from yyjson documentation):**

| Operation | jansson | yyjson | Speedup |
|-----------|---------|--------|---------|
| Parse (small) | 1.0x | 3.2x | 3.2x faster |
| Parse (large) | 1.0x | 3.5x | 3.5x faster |
| Serialize | 1.0x | 2.1x | 2.1x faster |
| Memory usage | 1.0x | 0.7x | 30% less |

**Relevance to ikigai:**

**Protocol Messages:**
- Current: Small JSON (~500 bytes) - Performance not critical
- Impact: Negligible (microseconds difference)

**OpenAI Streaming (Phase 3):**
- Future: Large SSE chunks, many JSON fragments
- Impact: **Significant** - Faster parsing = lower latency
- Streaming response parsing could be bottleneck

**Verdict:** Performance matters more in Phase 3+ (LLM integration) than current phase.

### 3. Simplified Error Handling ✅ MEDIUM VALUE

**Current:** Manual `json_decref()` in every error path

```c
json_t *root = json_loads(str, 0, &err);
if (!root) return ERR(ctx, PARSE, "...");

if (error1) {
    json_decref(root);  // Must remember
    return ERR(ctx, PARSE, "...");
}

if (error2) {
    json_decref(root);  // Must remember
    return ERR(ctx, PARSE, "...");
}
// ... 11 total json_decref() calls
```

**After Migration:** Automatic cleanup via talloc

```c
yyjson_doc *doc = yyjson_read_opts(str, len, 0, &talloc_alc, NULL);
if (!doc) return ERR(ctx, PARSE, "...");

// All error paths automatically clean up via talloc
if (error1) return ERR(ctx, PARSE, "...");
if (error2) return ERR(ctx, PARSE, "...");
// No manual cleanup needed
```

**Impact:** Fewer opportunities for memory leaks, simpler code

### 4. Remove Critical Bug Class ✅ MEDIUM VALUE

**Current Bug:** Functions that store `json_t*` must remember to `json_incref()`

**After Migration:** No reference counting = no incref/decref bugs possible

**Impact:** Entire class of ownership bugs eliminated

### 5. Smaller Codebase ✅ LOW-MEDIUM VALUE

**Estimate:** Remove ~50 lines of destructor boilerplate and error cleanup

**Impact:** Modest reduction, easier maintenance

---

## Risks and Costs

### 1. Migration Effort ⚠️ HIGH COST

**Code Changes Required:**
- Rewrite `src/protocol.c` parsing and serialization (~400 lines)
- Update `src/protocol.h` message structure
- Update `src/config.c` config parsing (~50 lines)
- Rewrite all protocol tests (~600 lines)
- Update `src/wrapper.c` mockable functions
- Update Makefile dependencies

**Estimated Time:**
- Development: 6-8 hours
- Testing: 2-3 hours
- Documentation: 1-2 hours
- **Total: 9-13 hours of focused work**

**Risk:** Time could expand if unexpected issues arise

### 2. Less Battle-Tested ⚠️ MEDIUM RISK

**jansson:**
- First release: 2009 (16 years of production use)
- Used by: Curl, Avahi, U-Boot, many enterprise systems
- CVEs: Few, quickly patched
- Maintainer: Active, responsive

**yyjson:**
- First release: 2020 (5 years of production use)
- Used by: Redis, ClickHouse, several large projects
- CVEs: None reported (as of 2025-11-12)
- Maintainer: Active, responsive (YaoYuan on GitHub)

**Concern:** Less time in production = higher chance of undiscovered bugs

**Mitigation:**
- yyjson has extensive test suite (98%+ coverage)
- Used in production by Redis (high-profile adoption)
- Actively maintained with quick bug fixes
- Memory safety focused (fuzz tested)

**Verdict:** Risk is moderate but manageable

### 3. API Differences ⚠️ MEDIUM COST

**jansson API (familiar):**
```c
json_t *obj = json_object_get(root, "key");
const char *str = json_string_value(obj);
```

**yyjson API (new):**
```c
yyjson_val *obj = yyjson_obj_get(root, "key");
const char *str = yyjson_get_str(obj);
```

**Learning Curve:**
- Similar concepts, different naming
- Need to learn mutable vs immutable documents
- Custom allocator setup (one-time complexity)

**Mitigation:**
- yyjson has excellent documentation
- API is intuitive and well-designed
- Wrapper functions can abstract differences

**Verdict:** Not a major concern, ~1 day to become proficient

### 4. Debian Package Availability ⚠️ LOW-MEDIUM RISK

**jansson:**
```bash
apt-get install libjansson-dev  # Available in all Debian releases
```

**yyjson:**
```bash
# NOT in Debian stable repositories (as of 2025-11-12)
# Must install from source or use pre-built binaries
```

**Concern:** Complicates build on fresh systems

**Mitigation Options:**

1. **Vendor yyjson source** (recommended)
   ```
   src/vendor/yyjson/
       yyjson.h
       yyjson.c
   ```
   - Single header + source (easy to vendor)
   - No external dependency
   - Always correct version

2. **Submodule approach**
   ```bash
   git submodule add https://github.com/ibireme/yyjson.git vendor/yyjson
   ```
   - Keep up-to-date with upstream
   - Requires git submodule management

3. **Download during build** (not recommended)
   - Brittle, requires network access
   - Breaks offline builds

**Verdict:** Vendoring is clean solution, but adds ~4KB to repository

### 5. Breaking Existing Code ⚠️ HIGH RISK (But Contained)

**Current State:**
- All JSON usage is in well-defined modules
- Comprehensive test coverage exists
- Clear module boundaries

**Risk Mitigation:**
- Tests will catch breaking changes
- Limited scope (only protocol and config modules)
- Can implement incrementally (config first, then protocol)

**Verdict:** Risk is high but well-contained by tests

### 6. Lost Type Safety ⚠️ LOW RISK

**jansson:**
```c
json_t *obj = json_object_get(root, "key");  // Type-specific functions
if (!json_is_string(obj)) { /* error */ }
const char *str = json_string_value(obj);
```

**yyjson:**
```c
yyjson_val *val = yyjson_obj_get(root, "key");  // Generic value
if (!yyjson_is_str(val)) { /* error */ }
const char *str = yyjson_get_str(val);
```

**Difference:** Both require runtime type checks, yyjson uses single `yyjson_val` type instead of distinct `json_t*` types.

**Verdict:** Minimal difference in type safety

---

## Technical Comparison

### Feature Matrix

| Feature | jansson | yyjson | Winner |
|---------|---------|--------|--------|
| **Memory Management** |
| Custom allocator | ❌ No | ✅ Yes | yyjson |
| Reference counting | ✅ Yes | ❌ No | jansson (or yyjson, depends on preference) |
| Zero-copy parsing | ❌ No | ✅ Yes | yyjson |
| Memory pooling | ❌ No | ✅ Yes | yyjson |
| **Performance** |
| Parse speed | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | yyjson |
| Serialize speed | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | yyjson |
| Memory usage | ⭐⭐⭐ | ⭐⭐⭐⭐ | yyjson |
| **API Quality** |
| Type safety | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | jansson |
| API simplicity | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | Tie |
| Documentation | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Tie |
| Error reporting | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | yyjson |
| **Maturity** |
| Years in production | 16 years | 5 years | jansson |
| Battle-tested | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | jansson |
| Maintenance | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | Tie |
| Community size | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | jansson |
| **Integration** |
| talloc integration | Destructors | Direct | yyjson |
| Package availability | ⭐⭐⭐⭐⭐ | ⭐⭐ | jansson |
| Vendoring ease | ⭐⭐ | ⭐⭐⭐⭐⭐ | yyjson (single file) |
| **Overall** | **36/50** | **41/50** | **yyjson (slight edge)** |

### API Comparison

#### Parsing

**jansson:**
```c
json_error_t err;
json_t *root = json_loads(json_str, 0, &err);
if (!root) {
    fprintf(stderr, "JSON error on line %d: %s\n", err.line, err.text);
    return NULL;
}

json_t *sess_id_json = json_object_get(root, "sess_id");
if (!json_is_string(sess_id_json)) {
    json_decref(root);
    return NULL;
}
const char *sess_id = json_string_value(sess_id_json);

json_decref(root);  // Manual cleanup
```

**yyjson:**
```c
yyjson_read_err err;
yyjson_alc alc = make_talloc_allocator(ctx);
yyjson_doc *doc = yyjson_read_opts(json_str, strlen(json_str), 0, &alc, &err);
if (!doc) {
    fprintf(stderr, "JSON error at position %zu: %s\n", err.pos, err.msg);
    return NULL;
}

yyjson_val *root = yyjson_doc_get_root(doc);
yyjson_val *sess_id_val = yyjson_obj_get(root, "sess_id");
if (!yyjson_is_str(sess_id_val)) {
    // No cleanup needed - talloc handles it
    return NULL;
}
const char *sess_id = yyjson_get_str(sess_id_val);

// No manual cleanup - talloc frees doc automatically
```

**Differences:**
- yyjson requires allocator setup (one-time cost)
- yyjson uses `yyjson_val` instead of `json_t`
- yyjson error reporting includes byte position (more precise)
- yyjson requires no manual cleanup when using talloc allocator

#### Serialization

**jansson:**
```c
json_t *root = json_object();
json_object_set_new(root, "sess_id", json_string(sess_id));
json_object_set_new(root, "type", json_string(type));

char *json_str = json_dumps(root, JSON_COMPACT);
json_decref(root);

// Later: free(json_str)
```

**yyjson:**
```c
yyjson_mut_doc *doc = yyjson_mut_doc_new(&talloc_alc);
yyjson_mut_val *root = yyjson_mut_obj(doc);
yyjson_mut_doc_set_root(doc, root);

yyjson_mut_obj_add_str(doc, root, "sess_id", sess_id);
yyjson_mut_obj_add_str(doc, root, "type", type);

size_t len;
char *json_str = yyjson_mut_write_opts(doc, YYJSON_WRITE_COMPACT, &talloc_alc, &len, NULL);

// No manual cleanup - talloc owns everything
```

**Differences:**
- yyjson separates immutable (`yyjson_doc`) and mutable (`yyjson_mut_doc`)
- yyjson uses `yyjson_mut_obj_add_*()` instead of `json_object_set_new()`
- yyjson serialization returns allocated string via custom allocator
- yyjson provides length output (useful for binary data)

#### Object Iteration

**jansson:**
```c
const char *key;
json_t *value;
json_object_foreach(root, key, value) {
    printf("%s: %s\n", key, json_string_value(value));
}
```

**yyjson:**
```c
yyjson_obj_iter iter = yyjson_obj_iter_with(root);
yyjson_val *key, *val;
while ((key = yyjson_obj_iter_next(&iter))) {
    val = yyjson_obj_iter_get_val(key);
    printf("%s: %s\n", yyjson_get_str(key), yyjson_get_str(val));
}
```

**Differences:**
- yyjson uses explicit iterator (more verbose but more control)
- jansson macro is more convenient for simple cases
- yyjson iterator can be paused/resumed

---

## Migration Strategy

### Phase 1: Preparation (1-2 hours)

1. **Research and Validation**
   - Complete research tasks (see "Research Required" section)
   - Build proof-of-concept with yyjson + talloc
   - Validate allocator integration works correctly

2. **Set Up yyjson**
   - Vendor yyjson source in `src/vendor/yyjson/`
   - Update Makefile to include yyjson
   - Verify builds on all target platforms

3. **Create Abstraction Layer**
   - Create `src/json_wrapper.{c,h}` to abstract yyjson API
   - Implement talloc allocator helper functions
   - Write tests for wrapper functions

### Phase 2: Migrate Config Module (1-2 hours)

**Rationale:** Config is simpler, good warm-up

1. **Update `src/config.c`**
   - Replace jansson parsing with yyjson
   - Test config loading from file
   - Verify all config tests pass

2. **Validation**
   - Run all config tests
   - Verify memory cleanup with valgrind
   - Check performance (should be faster)

### Phase 3: Migrate Protocol Module (3-4 hours)

**Rationale:** Most complex usage, requires careful migration

1. **Update `src/protocol.h`**
   - Change `json_t *payload` to `yyjson_doc *payload_doc` + `yyjson_val *payload`
   - Update message structure

2. **Update `src/protocol.c`**
   - Rewrite `ik_protocol_msg_parse()` with yyjson
   - Rewrite `ik_protocol_msg_serialize()` with yyjson
   - Rewrite `ik_protocol_msg_create_*()` functions
   - Remove destructor pattern (no longer needed)

3. **Update Tests**
   - Rewrite `tests/unit/protocol/*.c` for yyjson API
   - Update integration tests
   - Verify 100% coverage maintained

4. **Validation**
   - Run full test suite
   - Valgrind leak check
   - Performance benchmarks

### Phase 4: Cleanup and Documentation (1-2 hours)

1. **Remove jansson**
   - Remove jansson from Makefile
   - Remove jansson wrappers (`src/wrapper.c`)
   - Update build documentation

2. **Documentation**
   - Update `docs/memory.md` - Remove destructor pattern
   - Update `docs/architecture.md` - Document yyjson choice
   - Create `docs/yyjson_patterns.md` - Usage guide
   - Update `README.md` dependencies section

3. **Final Validation**
   - Full regression test suite
   - Build on clean system
   - Performance benchmarks vs jansson baseline

### Rollback Plan

**If migration fails or issues discovered:**

1. Keep jansson code in `git stash` or feature branch
2. Revert to jansson in < 1 hour (well-defined boundaries)
3. Document reasons for rollback in decision log

**Risk Mitigation:** Feature branch approach allows safe experimentation

---

## Code Changes Required

### Files to Modify

**Core Implementation:**
- `src/protocol.c` - Rewrite parsing/serialization (~400 lines)
- `src/protocol.h` - Update message structure (~10 lines)
- `src/config.c` - Rewrite config parsing (~50 lines)
- `src/wrapper.c` - Replace `ik_json_is_object_wrapper()` or remove (~10 lines)
- `Makefile` - Replace jansson with yyjson dependency (~2 lines)

**Tests:**
- `tests/unit/protocol/parse_test.c` - Update for yyjson API (~150 lines)
- `tests/unit/protocol/serialize_test.c` - Update for yyjson API (~100 lines)
- `tests/unit/protocol/create_test.c` - Update for yyjson API (~200 lines)
- `tests/unit/config_test.c` - Update for yyjson API (~50 lines)
- `tests/integration/protocol_integration_test.c` - Update (~100 lines)

**New Files:**
- `src/vendor/yyjson/yyjson.h` - Vendored header (~5000 lines, not written by us)
- `src/vendor/yyjson/yyjson.c` - Vendored implementation (~20000 lines, not written by us)
- `docs/yyjson_patterns.md` - Usage guide (~200 lines)

**Documentation:**
- `docs/memory.md` - Remove destructor pattern section (~30 lines)
- `docs/architecture.md` - Document yyjson decision (~20 lines)
- `docs/README.md` - Update dependencies (~5 lines)

**Total Lines Changed:** ~1,000 lines (excluding vendored yyjson source)

### Example: Before and After

#### Protocol Message Structure

**Before (jansson):**
```c
typedef struct {
    char *sess_id;
    char *corr_id;
    char *type;
    json_t *payload;  // jansson reference-counted
} ik_protocol_msg_t;

// Destructor required
static int ik_protocol_msg_destructor(ik_protocol_msg_t *msg)
{
    assert(msg != NULL);
    if (msg->payload) {
        json_decref(msg->payload);
        msg->payload = NULL;
    }
    return 0;
}
```

**After (yyjson):**
```c
typedef struct {
    char *sess_id;
    char *corr_id;
    char *type;
    yyjson_doc *payload_doc;  // talloc-managed document
    yyjson_val *payload;      // root value (borrowed from doc)
} ik_protocol_msg_t;

// No destructor needed - talloc handles everything
```

#### Parsing Function

**Before (jansson):**
```c
res_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str)
{
    json_error_t jerr;
    json_t *root = json_loads(json_str, 0, &jerr);
    if (!root) {
        return ERR(ctx, PARSE, "JSON parse error: %s", jerr.text);
    }

    if (ik_json_is_object_wrapper(root) == 0) {
        json_decref(root);  // Manual cleanup
        return ERR(ctx, PARSE, "Root JSON is not an object");
    }

    json_t *sess_id_json = json_object_get(root, "sess_id");
    if (!sess_id_json || !json_is_string(sess_id_json)) {
        json_decref(root);  // Manual cleanup
        return ERR(ctx, PARSE, "Missing or invalid sess_id");
    }

    // ... more parsing (9 more json_decref calls on error paths) ...

    ik_protocol_msg_t *msg = talloc_zero(ctx, ik_protocol_msg_t);
    if (!msg) {
        json_decref(root);
        return ERR(ctx, OOM, "Failed to allocate message");
    }

    talloc_set_destructor(msg, ik_protocol_msg_destructor);  // Set destructor

    msg->sess_id = talloc_strdup(msg, json_string_value(sess_id_json));
    // ... copy other fields ...

    json_t *payload_json = json_object_get(root, "payload");
    json_incref(payload_json);  // Take ownership
    msg->payload = payload_json;

    json_decref(root);  // Manual cleanup
    return OK(msg);
}
```

**After (yyjson):**
```c
res_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str)
{
    yyjson_read_err err;
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(json_str, strlen(json_str), 0, &alc, &err);
    if (!doc) {
        return ERR(ctx, PARSE, "JSON parse error at pos %zu: %s", err.pos, err.msg);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        // No manual cleanup - talloc handles doc
        return ERR(ctx, PARSE, "Root JSON is not an object");
    }

    yyjson_val *sess_id_val = yyjson_obj_get(root, "sess_id");
    if (!yyjson_is_str(sess_id_val)) {
        // No manual cleanup
        return ERR(ctx, PARSE, "Missing or invalid sess_id");
    }

    // ... more parsing (NO manual cleanup needed) ...

    ik_protocol_msg_t *msg = talloc_zero(ctx, ik_protocol_msg_t);
    if (!msg) {
        // No manual cleanup - talloc handles doc
        return ERR(ctx, OOM, "Failed to allocate message");
    }

    // No destructor needed

    msg->sess_id = talloc_strdup(msg, yyjson_get_str(sess_id_val));
    // ... copy other fields ...

    yyjson_val *payload_val = yyjson_obj_get(root, "payload");
    // Create sub-document for payload (owned by msg context)
    msg->payload_doc = doc;  // Transfer ownership to msg
    msg->payload = payload_val;

    return OK(msg);
}
```

**Key Differences:**
- ❌ **Removed:** 11 manual `json_decref(root)` calls
- ❌ **Removed:** Destructor setup
- ❌ **Removed:** `json_incref()` ownership dance
- ✅ **Added:** Custom allocator setup (one line)
- ✅ **Simplified:** Error paths auto-cleanup
- ✅ **Cleaner:** Single ownership model

---

## Research Required Before Decision

### Critical Research (Must Complete)

1. **Proof-of-Concept: talloc Allocator Integration**

   **Goal:** Verify yyjson custom allocator works seamlessly with talloc

   **Tasks:**
   - [ ] Create minimal example: parse JSON with talloc allocator
   - [ ] Verify memory allocated via talloc (use `talloc_report_full()`)
   - [ ] Confirm `talloc_free()` cleans up yyjson memory
   - [ ] Test error cases (parse failures don't leak)
   - [ ] Verify nested contexts work (child contexts)

   **Success Criteria:**
   - Zero memory leaks in valgrind
   - All JSON memory shows up in talloc reports
   - No manual cleanup required

2. **Performance Benchmarking: Real-World Data**

   **Goal:** Measure actual performance gain on ikigai workloads

   **Tasks:**
   - [ ] Benchmark protocol message parsing (current ~500 byte JSON)
   - [ ] Benchmark config file parsing (~1KB JSON)
   - [ ] Project LLM streaming workload (Phase 3: thousands of small JSON chunks)
   - [ ] Measure parsing latency (median, p95, p99)
   - [ ] Measure memory usage (peak, average)

   **Success Criteria:**
   - Measurable improvement on LLM streaming workload
   - No performance regression on protocol messages
   - Justify complexity of migration

3. **API Surface Analysis**

   **Goal:** Ensure yyjson API covers all current and planned use cases

   **Tasks:**
   - [ ] Map all jansson API calls in codebase to yyjson equivalents
   - [ ] Verify object/array iteration support
   - [ ] Confirm error handling capabilities
   - [ ] Check mutable document support (for serialization)
   - [ ] Validate streaming/incremental parsing (if needed for Phase 3)

   **Success Criteria:**
   - Every jansson usage has clear yyjson equivalent
   - No missing functionality for planned features

4. **Debian Packaging Research**

   **Goal:** Determine best approach for dependency management

   **Tasks:**
   - [ ] Check yyjson availability in Debian stable/testing/unstable
   - [ ] Research PPA availability
   - [ ] Evaluate vendoring approach (legal/license check)
   - [ ] Document build requirements for fresh systems

   **Success Criteria:**
   - Clear path for installing on Debian systems
   - No blockers for distribution

### Important Research (Should Complete)

5. **Battle-Testing Validation**

   **Goal:** Assess production readiness and risk

   **Tasks:**
   - [ ] Research production deployments (Redis, ClickHouse usage)
   - [ ] Review GitHub issues for critical bugs
   - [ ] Check CVE database for security issues
   - [ ] Analyze test coverage in yyjson repo
   - [ ] Review fuzzing results (if available)

   **Success Criteria:**
   - No show-stopping bugs in recent releases
   - Evidence of production use at scale
   - Active maintenance and quick bug fixes

6. **Migration Complexity Assessment**

   **Goal:** Validate time estimates are realistic

   **Tasks:**
   - [ ] Create detailed migration checklist
   - [ ] Estimate LOC changes per file
   - [ ] Identify potential migration pitfalls
   - [ ] Plan incremental migration strategy
   - [ ] Define rollback triggers

   **Success Criteria:**
   - High confidence in 9-13 hour estimate
   - Clear rollback plan if issues arise

7. **Error Handling Compatibility**

   **Goal:** Ensure yyjson error reporting meets requirements

   **Tasks:**
   - [ ] Compare error detail: jansson vs yyjson
   - [ ] Verify error position reporting
   - [ ] Test error messages are user-friendly
   - [ ] Confirm integration with `res_t` error system

   **Success Criteria:**
   - Error reporting is equal or better than jansson
   - Integrates cleanly with existing error handling

### Nice-to-Have Research

8. **Community and Support**

   **Tasks:**
   - [ ] Review maintainer responsiveness on GitHub
   - [ ] Check mailing list/forum activity
   - [ ] Assess documentation quality
   - [ ] Look for existing talloc integration examples

   **Success Criteria:**
   - Active community support
   - Good documentation for migration

9. **Long-Term Maintenance**

   **Tasks:**
   - [ ] Review release cadence
   - [ ] Check backward compatibility policy
   - [ ] Assess API stability
   - [ ] Look for deprecation warnings

   **Success Criteria:**
   - Stable, well-maintained library
   - Low risk of breaking changes

---

## Decision Criteria

### Go/No-Go Checklist

**Proceed with migration if:**

- [x] ✅ Proof-of-concept demonstrates clean talloc integration
- [ ] ⚠️ Performance benchmarks show **significant** improvement on LLM streaming (Phase 3+)
- [x] ✅ All current jansson usage maps cleanly to yyjson
- [x] ✅ No critical bugs or CVEs in yyjson
- [ ] ⚠️ Time estimate validated (9-13 hours realistic)
- [x] ✅ Clear dependency management strategy (vendoring acceptable)
- [ ] ⚠️ Team consensus on value vs cost trade-off

**DO NOT proceed if:**

- [ ] Proof-of-concept reveals talloc integration issues
- [ ] Performance gains are negligible (< 20% improvement)
- [ ] Migration reveals missing yyjson functionality
- [ ] Critical bugs discovered in yyjson
- [ ] Time estimate exceeds 20 hours
- [ ] Team prefers simplicity over performance

### Trigger Points for Decision

**Decide NOW (before Phase 3) if:**
- Performance is critical for LLM streaming
- Want to eliminate jansson ownership bug permanently
- Team has bandwidth for 9-13 hour migration

**Defer decision (after Phase 3) if:**
- Current jansson implementation (with bug fix) is adequate
- Performance not yet a bottleneck
- Want to focus on feature development

**Never migrate if:**
- Proof-of-concept fails
- yyjson reveals critical issues
- Cost exceeds benefit

---

## Recommendation

### Primary Recommendation: **DO NOT MIGRATE NOW**

**Rationale:**

1. **Fix jansson bug first** - The critical `json_incref()` bug is easily fixed (1 line change)
2. **Jansson works fine** - After bug fix, jansson is correct and adequate
3. **Performance not yet critical** - Current protocol messages are tiny (~500 bytes)
4. **Phase 3 is unknown** - LLM streaming requirements unclear until implemented
5. **Time better spent elsewhere** - 9-13 hours better spent on features
6. **Battle-tested matters** - jansson's 16 years in production is valuable
7. **Destructor pattern is fine** - It's a clean, correct integration with talloc

**Action Items:**

1. ✅ Fix `ik_protocol_msg_create_assistant_resp()` - Add `json_incref(payload)`
2. ✅ Document jansson ownership patterns in `docs/jansson_patterns.md`
3. ✅ Add ownership annotations to function headers
4. ⏸️ Defer yyjson migration decision until Phase 3

### Alternative Recommendation: **MIGRATE DURING PHASE 3**

**If Phase 3 reveals performance bottlenecks in JSON parsing:**

1. Complete critical research (proof-of-concept, benchmarks)
2. Validate performance gains justify migration cost
3. Allocate focused 2-day sprint for migration
4. Keep jansson branch as rollback option

**Triggers for reconsidering:**
- LLM streaming reveals JSON parsing as bottleneck (profiler confirms)
- Team wants cleaner talloc-only memory model
- Destructor pattern proves problematic in practice

### Tertiary Recommendation: **MIGRATE NOW (If Team Prefers)**

**Only if:**
- Team philosophically prefers single memory model
- Willing to invest 9-13 hours upfront
- Want to eliminate entire class of refcounting bugs
- Performance matters for future-proofing

**Conditions:**
- Complete critical research first (proof-of-concept mandatory)
- Budget 2 full days for migration + testing
- Have rollback plan ready

---

## Timeline

### If "Do Not Migrate Now" (Recommended)

**Immediate (Week 1):**
- Fix jansson ownership bug
- Document jansson patterns
- Close this proposal (deferred)

**Phase 3 (LLM Integration):**
- Monitor JSON parsing performance
- Profile LLM streaming workload
- Revisit decision if bottleneck identified

**Phase 4+ (Optimization):**
- Consider migration if performance critical
- Re-evaluate with production data

### If "Migrate During Phase 3"

**Phase 3 Start:**
- Complete critical research (1-2 days)
- Make go/no-go decision

**If Go:**
- Allocate 2-day sprint for migration
- Migrate in phases (config → protocol → tests)
- Validate performance improvement

### If "Migrate Now"

**Week 1:**
- Day 1: Complete critical research, proof-of-concept
- Day 2: Go/no-go decision

**Week 2 (If Go):**
- Day 1: Setup + config migration
- Day 2: Protocol migration + tests
- Day 3: Validation + documentation + cleanup

**Week 3:**
- Final testing and integration

---

## Appendix A: talloc Allocator Implementation

### Minimal Allocator Wrapper

```c
// In src/json_allocator.h
typedef struct {
    yyjson_alc alc;        // yyjson allocator interface
    TALLOC_CTX *ctx;       // talloc context
} ik_json_allocator_t;

// Create allocator for given talloc context
ik_json_allocator_t ik_make_json_allocator(TALLOC_CTX *ctx);

// In src/json_allocator.c
static void *json_talloc_malloc(void *ctx, size_t size)
{
    TALLOC_CTX *tctx = (TALLOC_CTX *)ctx;
    return talloc_size(tctx, size);
}

static void *json_talloc_realloc(void *ctx, void *ptr, size_t size)
{
    TALLOC_CTX *tctx = (TALLOC_CTX *)ctx;
    return talloc_realloc_size(tctx, ptr, size);
}

static void json_talloc_free(void *ctx, void *ptr)
{
    talloc_free(ptr);
}

ik_json_allocator_t ik_make_json_allocator(TALLOC_CTX *ctx)
{
    ik_json_allocator_t allocator = {
        .alc = {
            .malloc = json_talloc_malloc,
            .realloc = json_talloc_realloc,
            .free = json_talloc_free,
            .ctx = ctx
        },
        .ctx = ctx
    };
    return allocator;
}
```

### Usage Pattern

```c
res_t parse_json(TALLOC_CTX *ctx, const char *json_str)
{
    ik_json_allocator_t allocator = ik_make_json_allocator(ctx);

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts(
        json_str,
        strlen(json_str),
        0,
        &allocator.alc,
        &err
    );

    if (!doc) {
        return ERR(ctx, PARSE, "JSON parse error: %s", err.msg);
    }

    // doc is now talloc-managed, no destructor needed
    return OK(doc);
}
```

---

## Appendix B: Performance Benchmarks (To Be Completed)

### Benchmark Plan

```c
// Benchmark harness
typedef struct {
    const char *name;
    const char *json_data;
    size_t iterations;
} benchmark_t;

benchmark_t benchmarks[] = {
    {
        .name = "Protocol message (small)",
        .json_data = "{\"sess_id\":\"123\",\"corr_id\":\"456\","
                     "\"type\":\"user_msg\",\"payload\":{\"content\":\"hello\"}}",
        .iterations = 100000
    },
    {
        .name = "Config file (medium)",
        .json_data = /* 1KB config file */,
        .iterations = 10000
    },
    {
        .name = "LLM streaming chunk (tiny)",
        .json_data = "{\"id\":\"1\",\"object\":\"chat.completion.chunk\","
                     "\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}",
        .iterations = 1000000  // Simulate high-frequency streaming
    }
};

void run_benchmarks() {
    for (size_t i = 0; i < sizeof(benchmarks) / sizeof(benchmark_t); i++) {
        benchmark_t *b = &benchmarks[i];

        // Benchmark jansson
        uint64_t jansson_time = benchmark_jansson(b->json_data, b->iterations);

        // Benchmark yyjson
        uint64_t yyjson_time = benchmark_yyjson(b->json_data, b->iterations);

        printf("%s:\n", b->name);
        printf("  jansson: %lu ns/op\n", jansson_time / b->iterations);
        printf("  yyjson:  %lu ns/op\n", yyjson_time / b->iterations);
        printf("  speedup: %.2fx\n", (double)jansson_time / yyjson_time);
    }
}
```

**Expected Results (based on published benchmarks):**
- Protocol message: 2-3x faster
- Config file: 2-3x faster
- LLM streaming: 3-4x faster (critical workload)

**Actual results:** To be measured during research phase

---

## Appendix C: Migration Checklist

### Pre-Migration

- [ ] Complete critical research tasks
- [ ] Run proof-of-concept
- [ ] Obtain team consensus
- [ ] Create feature branch: `feature/yyjson-migration`
- [ ] Tag current commit: `pre-yyjson-migration`

### Migration Phase 1: Setup

- [ ] Vendor yyjson source in `src/vendor/yyjson/`
- [ ] Update Makefile to include yyjson
- [ ] Create `src/json_allocator.{c,h}`
- [ ] Verify clean build

### Migration Phase 2: Config Module

- [ ] Update `src/config.c` to use yyjson
- [ ] Run config tests (all pass)
- [ ] Valgrind check (no leaks)

### Migration Phase 3: Protocol Module

- [ ] Update `src/protocol.h` structure
- [ ] Rewrite `ik_protocol_msg_parse()`
- [ ] Rewrite `ik_protocol_msg_serialize()`
- [ ] Rewrite `ik_protocol_msg_create_err()`
- [ ] Rewrite `ik_protocol_msg_create_assistant_resp()`
- [ ] Remove destructor pattern
- [ ] Update `tests/unit/protocol/parse_test.c`
- [ ] Update `tests/unit/protocol/serialize_test.c`
- [ ] Update `tests/unit/protocol/create_test.c`
- [ ] Update `tests/integration/protocol_integration_test.c`
- [ ] Run all tests (100% pass)
- [ ] Valgrind check (no leaks)
- [ ] Verify 100% coverage maintained

### Migration Phase 4: Cleanup

- [ ] Remove jansson from Makefile
- [ ] Remove `src/wrapper.c` (if no longer needed)
- [ ] Update `docs/memory.md`
- [ ] Create `docs/yyjson_patterns.md`
- [ ] Update `docs/architecture.md`
- [ ] Update `README.md` dependencies

### Validation

- [ ] Full test suite passes
- [ ] Valgrind: zero leaks
- [ ] Coverage: 100% maintained
- [ ] Performance: benchmarks show improvement
- [ ] Build: clean system build succeeds
- [ ] Documentation: complete and accurate

### Completion

- [ ] Merge feature branch to main
- [ ] Tag commit: `post-yyjson-migration`
- [ ] Update `docs/decisions/` with ADR (Architecture Decision Record)
- [ ] Close this proposal (implemented)

---

## Appendix D: References

### yyjson Resources

- **GitHub:** https://github.com/ibireme/yyjson
- **Documentation:** https://ibireme.github.io/yyjson/doc/doxygen/html/
- **Benchmarks:** https://github.com/ibireme/yyjson#benchmark
- **License:** MIT (compatible with ikigai)

### jansson Resources

- **GitHub:** https://github.com/akheron/jansson
- **Documentation:** https://jansson.readthedocs.io/
- **License:** MIT (compatible with ikigai)

### Related Documentation

- `docs/memory.md` - Current memory management patterns
- `docs/memory_usage_analysis.md` - Analysis identifying jansson issues
- `docs/jansson_patterns.md` - Safe patterns for jansson (to be created)
- `docs/architecture.md` - System architecture

### Benchmarks

- **nativejson-benchmark:** https://github.com/miloyip/nativejson-benchmark
- **JSON parsing performance:** https://github.com/simdjson/simdjson/blob/master/benchmark/README.md

---

## Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-11-12 | Claude (Sonnet 4.5) | Initial proposal |

---

**Next Steps:**

1. Review this proposal with project maintainer (mgreenly)
2. If approved for research: Complete critical research tasks
3. If research validates: Make go/no-go decision
4. If go: Execute migration following checklist
5. If no-go: Fix jansson bug, defer decision, close proposal

**Status:** Awaiting review and decision
