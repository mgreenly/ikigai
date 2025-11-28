# Memory Management Analysis

**Date:** 2025-11-12
**Status:** HISTORICAL - Analysis completed, informed yyjson migration decision
**Overall Assessment:** WORKABLE with refinements needed
**Note:** This analysis references `protocol.c` which was removed in Phase 2.5 (2025-11-13). Findings remain valid for `config.c` and future JSON usage.

**OOM Handling Update (2025-11-14):** After this analysis was completed, the project changed OOM handling from recoverable errors to PANIC (abort). References to ERR_OOM in this document are historical.

---

## Executive Summary

The memory management strategy is **fundamentally sound** but mixing jansson's reference counting with talloc's hierarchical ownership creates complexity and potential bugs.

**Key Finding:** Lifecycle mismatch between talloc (hierarchical) and jansson (reference counting) is the primary concern.

**Resolution:** Approved migration to yyjson (see `jansson_to_yyjson_proposal.md`) eliminates this issue.

---

## Strengths

### 1. Excellent talloc Understanding ✅
- Correct parent-child relationships throughout
- Proper use of `talloc_new()`, `talloc_zero()`, `talloc_strdup()`
- Tree-based cleanup correctly leveraged
- No premature use of advanced features

### 2. Strong Error/Memory Integration ✅
- `res_t` errors allocated on caller's context
- OOM causes PANIC (abort) instead of returning error (updated 2025-11-14)
- `TRY` and `CHECK` macros properly propagate errors
- Error paths correctly free resources

### 3. Comprehensive Testing ✅
- OOM injection infrastructure removed (2025-11-14) - OOM now causes PANIC
- Assertion testing with `SIGABRT` validation
- Good coverage of error paths (non-OOM)

---

## Critical Issues

### 1. ⚠️ MAJOR: Mixed Memory Management (jansson + talloc)

**Problem:** Protocol module mixes talloc-allocated memory with jansson's reference-counted `json_t` objects.

**Why Concerning:**
- **Lifecycle mismatch:** talloc uses hierarchical ownership, jansson uses reference counting
- **Destructor reliance:** Must remember `talloc_set_destructor()` or leak JSON memory
- **Reference count errors:** Single `json_incref`/`json_decref` mismatch causes bugs

**Specific Bug Found (`src/protocol.c:289-291`):**
```c
// BUG: Takes ownership of payload but doesn't json_incref()
msg->payload = payload;
// When destructor runs, it will json_decref() an object we don't own!
```

**Resolution:** yyjson migration eliminates reference counting entirely. ✅

### 2. ⚠️ MEDIUM: Destructor Pattern Complexity

**Current Pattern:**
```c
static int ik_protocol_msg_destructor(ik_protocol_msg_t *msg) {
    if (msg->payload) {
        json_decref(msg->payload);  // Must manually manage JSON lifecycle
    }
    return 0;
}
```

**Issues:**
- Boilerplate required for every struct containing JSON
- Error-prone (easy to forget destructor setup)
- Cleanup order dependencies

**Resolution:** yyjson eliminates need for destructors. ✅

### 3. ⚠️ MEDIUM: Error Path Cleanup Burden

**Current:** 11 manual `json_decref(root)` calls in `ik_protocol_msg_parse()`

**Issues:**
- Easy to miss cleanup in error paths
- Increases code complexity
- More test coverage required

**Resolution:** yyjson + talloc provides automatic cleanup. ✅

---

## Medium Concerns

### 1. Memory Pools Not Used
- **Current:** Every allocation goes through talloc
- **Consideration:** For high-frequency allocations (e.g., LLM streaming), memory pools could reduce overhead
- **Verdict:** Premature optimization - defer until profiling identifies need

### 2. No Memory Limit Enforcement
- **Current:** No caps on memory usage
- **Future:** May want limits for long-running sessions
- **Action:** Defer to production monitoring

### 3. talloc Reports Not Integrated
- **Current:** No production memory diagnostics
- **Future:** Add `talloc_report_full()` to debug builds
- **Priority:** Low - wait for need

---

## Minor Concerns

### 1. Function Ownership Documentation
- **Issue:** Not all functions clearly document ownership transfer
- **Example:** Is `payload` parameter owned by caller or callee?
- **Fix:** Add ownership annotations to function docs

### 2. Const Correctness
- **Issue:** Some `char *` params could be `const char *`
- **Impact:** Low - code works correctly
- **Fix:** Gradual improvement as code evolves

---

## Talloc-Specific Pitfalls (Future Awareness)

### Pitfalls NOT Currently Present (Good!) ✅
- ✅ No use of `talloc_steal()` (avoid complexity)
- ✅ No use of `talloc_reference()` (avoid confusion)
- ✅ No long-lived temporary contexts (correct patterns)
- ✅ No improper NULL parent usage (all contexts well-scoped)

### Watch Out For (As Codebase Grows)
- **Circular references:** talloc can handle but adds complexity
- **Context lifetime confusion:** Ensure child outlive parents conceptually
- **Pool overuse:** Don't add pools prematurely

---

## Recommendations

### Immediate (Pre-Phase 3)

1. **✅ DONE: Migrate to yyjson**
   - Eliminates jansson reference counting
   - Resolves lifecycle mismatch
   - Simplifies error paths
   - See `jansson_to_yyjson_proposal.md` (APPROVED)

2. **Add ownership annotations**
   - Document which functions transfer ownership
   - Example: `// Takes ownership of payload`
   - Reduces cognitive load

3. **Review all destructor uses**
   - After yyjson migration, most should be eliminated
   - Only keep destructors for genuine cleanup (e.g., file handles, sockets)

### Medium-Term (Phase 3+)

4. **Profile memory usage during LLM streaming**
   - Identify high-frequency allocations
   - Consider pools if profiling justifies

5. **Add memory diagnostics to debug builds**
   - Integrate `talloc_report_full()`
   - Helps catch leaks during development

6. **Document talloc patterns**
   - Create `docs/talloc_patterns.md`
   - Capture best practices for new code

### Long-Term (v1.0+)

7. **Memory usage limits**
   - Add configurable caps for production
   - Graceful degradation when limits hit

8. **Valgrind integration in CI**
   - Already used manually, automate in CI pipeline
   - Catch leaks before merging

---

## Verdict

**Before yyjson migration:** WORKABLE but complex (jansson + talloc mixing)

**After yyjson migration (APPROVED):** EXCELLENT
- Single memory model (talloc-only)
- No destructors needed for JSON
- Automatic cleanup in error paths
- Simpler architecture

**Confidence:** HIGH - talloc fundamentals are solid, yyjson resolves main concern

---

## Key Patterns (Current Implementation)

### Correct Pattern: Error Propagation
```c
res_t func(TALLOC_CTX *ctx) {
    TALLOC_CTX *tmp_ctx = talloc_new(ctx);

    res_t result = do_something(tmp_ctx);
    if (is_err(&result)) {
        talloc_free(tmp_ctx);  // Clean up temp context
        return result;         // Propagate error
    }

    // Success: steal to parent context
    void *data = talloc_steal(ctx, result.ok);
    talloc_free(tmp_ctx);
    return OK(data);
}
```

### Correct Pattern: OOM Handling (Updated 2025-11-14)
```c
void *ptr = talloc_zero_(ctx, size);
if (!ptr) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
```

**Historical note:** This pattern previously returned `ERR(ctx, OOM, ...)` but now uses PANIC for immediate termination.

### Correct Pattern: Child Contexts
```c
TALLOC_CTX *ctx = talloc_new(parent);
if (!ctx) return ERR(parent, OOM, "...");

// Use ctx for temporary allocations
// ...

talloc_free(ctx);  // Frees all children automatically
```

---

## References

- **talloc documentation:** https://talloc.samba.org/talloc/doc/html/index.html
- **Related:** `docs/memory.md` - Memory management philosophy
- **Related:** `docs/error_handling.md` - Error system integration
- **Related:** `docs/jansson_to_yyjson_proposal.md` - Resolution for jansson issues

---

**Next Steps:**
1. ✅ yyjson migration approved
2. Document ownership annotations
3. Update `docs/memory.md` after migration

**Status:** Analysis complete - Informed yyjson migration decision
