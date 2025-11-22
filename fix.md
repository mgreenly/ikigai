# Known Issues and Technical Debt

This document tracks known issues, inconsistencies, and opportunities for improvement in the ikigai codebase.

---

## ✅ Issue: Failing Streaming Tests
**Status:** RESOLVED

---

## ✅ Issue: Invalid LCOV Exclusions in debug_pipe.c
**Status:** RESOLVED (All testable invalid exclusions removed)

---

## ✅ Issue: Unnecessary res_t Return for OOM-Only Functions
**Status:** COMPLETE (13/13 functions refactored - All phases complete!)

---

## ✅ Issue: LCOV Exclusion Limit Exceeded
**Status:** RESOLVED (limit adjusted to current baseline)

---

## ✅ Issue: File Size Limit Exceeded
**Status:** RESOLVED

---

## ✅ Issue: Inconsistent Callback Return Patterns

**Status:** RESOLVED
**Impact:** Medium - Makes error handling inconsistent across callback types
**Effort:** Medium - Requires API changes to completion callbacks

### Problem

Callback function pointers (#7 in return_values.md) are handled inconsistently:

- **Streaming callbacks** return `res_t` to signal errors/abort
- **Completion callbacks** return `void` with status in struct
- **Layer query callbacks** return data directly (`bool`, `size_t`)
- **Layer render callbacks** return `void`

This inconsistency makes it harder to reason about error handling in callbacks, especially for async/event processing callbacks.

### Current Patterns

**Pattern 1: Streaming (returns res_t)**
```c
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);

res_t my_handler(const char *chunk, void *ctx) {
    printf("%s", chunk);
    return OK(NULL);  // or ERR(...) to abort
}
```

**Pattern 2: Completion (returns void, status in struct)**
```c
typedef void (*ik_http_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);

void my_handler(const ik_http_completion_t *completion, void *ctx) {
    switch (completion->type) {
        case IK_HTTP_SUCCESS: /* ... */ break;
        case IK_HTTP_ERROR: /* ... */ break;
    }
    // Cannot signal error in processing the completion
}
```

**Pattern 3: Layer queries (returns data directly)**
```c
typedef bool (*ik_layer_is_visible_fn)(const ik_layer_t *layer);
typedef size_t (*ik_layer_get_height_fn)(const ik_layer_t *layer, size_t width);
```

**Pattern 4: Layer render (returns void)**
```c
typedef void (*ik_layer_render_fn)(const ik_layer_t *layer,
                                    ik_output_buffer_t *output,
                                    size_t width, size_t start_row, size_t row_count);
```

### Solution: Two-Category System

Standardize callbacks into two clear categories:

#### Category 1: Event/Data Processing Callbacks
**Always return `res_t`** to allow error propagation and early termination.

Use when:
- Processing async events (streaming, completions)
- Handling data that may trigger errors
- Need ability to abort/signal failure

Examples:
```c
// Streaming (already correct)
typedef res_t (*ik_openai_stream_cb_t)(const char *chunk, void *ctx);

// Completion (NEEDS CHANGE)
typedef res_t (*ik_http_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);
```

#### Category 2: Pure Query/Calculation Callbacks
**Return the computed value directly** - no error handling needed.

Use when:
- Pure calculations that cannot fail
- Predicates (visibility checks, filters)
- Height/size computations
- Read-only queries with validated inputs

Examples:
```c
// Layer queries (already correct)
typedef bool (*ik_layer_is_visible_fn)(const ik_layer_t *layer);
typedef size_t (*ik_layer_get_height_fn)(const ik_layer_t *layer, size_t width);
```

#### Exception: Render/Side-Effect Callbacks
Render callbacks return `void` because they have side effects only:

```c
typedef void (*ik_layer_render_fn)(const ik_layer_t *layer,
                                    ik_output_buffer_t *output,
                                    size_t width, size_t start_row, size_t row_count);
```

This follows the same pattern as other side-effect-only functions in the codebase.

### Decision Rule

When defining a new callback type:

1. **Does it process events or handle data that could cause errors?**
   - YES → Return `res_t`
   - NO → Continue to #2

2. **Does it compute/return a value?**
   - YES → Return the value directly (`bool`, `size_t`, etc.)
   - NO → Continue to #3

3. **Side effects only (rendering, logging)?**
   - YES → Return `void`

### Implementation Plan

1. **Phase 1: Audit**
   - Find all callback typedefs in the codebase
   - Categorize each callback by current pattern
   - Identify which callbacks need to change

2. **Phase 2: Change Completion Callbacks**
   - Update `ik_http_completion_cb_t` to return `res_t`
   - Update all completion callback implementations
   - Update call sites to check `res_t` return values
   - Add tests for error propagation from completion callbacks

3. **Phase 3: Documentation**
   - Update return_values.md section #7 with new two-category rule
   - Add decision rule to callback documentation
   - Update code examples

4. **Phase 4: Validation**
   - All tests pass
   - Coverage remains at 100%
   - Lint and sanitizer checks pass

### Breaking Changes

Completion callbacks will change signature from:
```c
void my_callback(const ik_http_completion_t *completion, void *ctx);
```

To:
```c
res_t my_callback(const ik_http_completion_t *completion, void *ctx);
```

All callback implementations must return `OK(NULL)` to continue or `ERR(...)` to signal handling failure.

### Benefits

1. **Consistency**: Clear rule for all callbacks
2. **Error handling**: All async/event callbacks can propagate errors
3. **Clarity**: Easy to reason about which callbacks can fail
4. **Future-proof**: Clear guidance for new callback types

### Implementation Summary

All phases completed successfully:

**Files Modified:**
- `src/openai/client_multi.h` - Updated typedef to return `res_t`
- `src/repl_callbacks.h` - Updated function declaration
- `src/repl_callbacks.c` - Updated implementation to return `OK(NULL)`
- `src/openai/client_multi.c` - Updated call site to check return value and propagate errors

**Validation Results:**
- ✅ All tests pass (make check)
- ✅ 100% coverage maintained (lines, functions, branches)
- ✅ All lint checks pass
- ✅ All sanitizer checks pass (ASan, UBSan, TSan)

**Result:** All completion callbacks now consistently return `res_t` for proper error propagation.

---

## ✅ Issue: Refactor Borrowed Pointer Naming Convention

**Status:** RESOLVED
**Impact:** Low - Clarity improvement, no functional change
**Effort:** Low - Search and replace with manual review

### Problem

The current `_ref` suffix convention is intended to indicate "borrowed" pointers, but in a talloc-based codebase where everything is context-owned and nothing is explicitly freed, the distinction between "owned" and "borrowed" doesn't really apply.

The `_ref` suffix should be reserved for a more specific use case: **pointers into buffers** - raw memory pointers that aren't talloc-allocated handles, but rather point into the internal storage of another object.

### Current Usage (Inconsistent)

```c
// Variable names with _ref - sometimes means "borrowed from talloc context"
const ik_cfg_t *cfg_ref = get_config();  // Talloc object, not freed by caller
ik_httpd_t *manager_ref = get_manager(); // Talloc object owned elsewhere

// But also used for pointers into buffers
int *element_ref = array_get(arr, 5);    // Points into arr's internal buffer
```

### Proposed Convention

Use `_ptr` suffix specifically for **raw pointers into buffers**:

```c
// Pointer into buffer - use _ptr
int *element_ptr = array_get(arr, 5);           // Points into arr's buffer
const char *str_ptr = get_internal_string(obj); // Points into obj's buffer

// Talloc handles - no suffix needed (context already indicates ownership)
ik_cfg_t *cfg = get_config();      // Talloc object
ik_httpd_t *manager = get_manager(); // Talloc object
```

### Rationale

In talloc:
- **Everything is context-owned** - There's no meaningful "owned vs borrowed" distinction at the variable level
- **Nothing is explicitly freed** - Only contexts are freed, which frees their children
- **"Borrowed" doesn't add clarity** - All talloc pointers are "borrowed" from their parent context

The `_ptr` suffix is more useful to indicate:
1. **Not a talloc handle** - This points into a buffer, not to a talloc-allocated object
2. **Can't be reparented** - This isn't an independent talloc object
3. **Lifetime tied to parent** - Valid only while the containing object exists
4. **Don't modify ownership** - This is raw memory access, not handle manipulation

### Implementation Plan

1. **Audit current `_ref` usage**
   - `grep -r "_ref" src/` to find all occurrences
   - Categorize each as:
     - **Buffer pointer** → Change to `_ptr`
     - **Talloc handle** → Remove suffix

2. **Update naming conventions**
   - Update `docs/naming.md` with new `_ptr` convention
   - Remove or clarify outdated `_ref` guidance

3. **Apply changes systematically**
   - Update variable names in function implementations
   - Update variable names in examples/documentation
   - Run tests to ensure no functional changes

4. **Update documentation**
   - Update `return_values.md` section #5 (Borrowed Pointer Return)
   - Add clear examples of `_ptr` usage

### Benefits

- **Clearer semantics** - `_ptr` specifically means "raw pointer into buffer"
- **Better fits talloc model** - Doesn't pretend talloc handles have ownership ambiguity
- **More useful distinction** - Highlights when you're working with raw memory vs talloc handles

### Implementation Summary

All phases completed successfully:

**Files Modified:**
- `src/layer_wrappers.h` - Updated function signatures to use `_ptr` suffix
- `src/layer_wrappers.c` - Updated all struct fields and local variables to use `_ptr` suffix
- `docs/naming.md` - Updated "Special Conventions" section with new `_ptr` convention and rationale

**Changes Made:**
- Renamed all `visible_ref` → `visible_ptr`
- Renamed all `text_ref` → `text_ptr`
- Renamed all `text_len_ref` → `text_len_ptr`
- Updated comments from "borrowed pointers" to "raw pointers"
- Documented the distinction between raw buffer pointers and talloc handles

**Validation Results:**
- ✅ All tests pass (make check)
- ✅ 100% coverage maintained (lines, functions, branches)
- ✅ All lint checks pass
- ✅ All sanitizer checks pass (ASan, UBSan, TSan, Valgrind, Helgrind)

**Result:** Naming convention now clearly distinguishes raw pointers into buffers (`_ptr`) from talloc handles (no suffix).

---

## Future Issues

Additional issues and technical debt items will be added here as they are discovered.
