# Known Issues and Technical Debt

This document tracks known issues, inconsistencies, and opportunities for improvement in the ikigai codebase.

---

## Issue: Unnecessary res_t Return for OOM-Only Functions

**Status:** Not yet addressed
**Impact:** Medium - Adds boilerplate without providing real error handling
**Effort:** Low-Medium - Requires API changes and call site updates

### Problem

Some functions return `res_t` with output parameters (`**_out`) but only fail on out-of-memory conditions, which always PANIC. These functions never return ERR to be handled by callers - they either succeed or abort the process.

This pattern adds unnecessary complexity:
- Callers must check `res_t` even though errors are never handled gracefully
- Call sites uniformly PANIC on error anyway
- The function signature is more complex than needed
- Creates false impression that errors might be recoverable

### Example

**Current implementation:**
```c
// format.c
res_t ik_format_buffer_create(void *parent, ik_format_buffer_t **buf_out)
{
    assert(parent != NULL);   // LCOV_EXCL_BR_LINE
    assert(buf_out != NULL);  // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = talloc_zero_(parent, sizeof(ik_format_buffer_t));
    if (buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_byte_array_create(buf, 32);
    if (is_err(&res)) PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
    buf->array = res.ok;

    *buf_out = buf;
    return OK(buf);  // Never returns ERR
}
```

**Current call site:**
```c
ik_format_buffer_t *buf = NULL;
res_t result = ik_format_buffer_create(repl, &buf);
if (is_err(&result)) PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
```

**Simplified implementation:**
```c
// format.c
ik_format_buffer_t *ik_format_buffer_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = talloc_zero_(parent, sizeof(ik_format_buffer_t));
    if (buf == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_byte_array_create(buf, 32);
    if (is_err(&res)) PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
    buf->array = res.ok;

    return buf;
}
```

**Simplified call site:**
```c
ik_format_buffer_t *buf = ik_format_buffer_create(repl);
```

### When res_t IS Appropriate

Functions that have **real error conditions** (IO, parsing, validation) should absolutely use `res_t`:

**Good use of res_t:**
```c
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)
{
    // Can fail opening /dev/tty
    int tty_fd = posix_open_("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        return ERR(parent, IO, "Failed to open /dev/tty");
    }

    // Can fail getting terminal attributes
    if (posix_tcgetattr_(tty_fd, &ctx->orig_termios) < 0) {
        posix_close_(tty_fd);
        return ERR(parent, IO, "Failed to get terminal attributes");
    }
    // ... more real error conditions
}
```

**Caller handles errors gracefully:**
```c
res_t result = ik_term_init(repl, &repl->term);
if (is_err(&result)) {
    talloc_free(repl);
    return result;  // Propagates error - caller might log, retry, etc.
}
```

### Identification Criteria

A function is a candidate for simplification if:

1. ✅ Returns `res_t` with output parameter pattern
2. ✅ Only fails on memory allocation (OOM)
3. ✅ All error paths PANIC (never returns ERR)
4. ✅ All call sites PANIC on error (never handle gracefully)

A function should KEEP `res_t` if:

1. Has IO operations that can fail (file, network, terminal)
2. Has parsing that can fail on malformed input
3. Has validation that can reject bad input
4. Any error condition that could be handled by caller

### Resolution Plan

When addressing this issue:

1. **Search exhaustively** for all functions matching the criteria
   - `grep -r "res_t.*\*\*.*)" src/` to find candidates
   - Inspect each function's implementation
   - Check if any error path returns ERR (vs PANIC)
   - Check all call sites to see if errors are handled

2. **Categorize functions:**
   - Group A: Only PANIC on OOM → simplify
   - Group B: Real error conditions → keep res_t

3. **Refactor in phases:**
   - Update function signature (remove `res_t`, remove output param)
   - Update implementation (direct return)
   - Update all call sites
   - Update tests
   - Verify 100% coverage maintained

4. **Update documentation:**
   - Update `return_values.md` with clearer guidance
   - Add examples of when NOT to use output parameter pattern

### Other Potential Candidates

Based on initial investigation, likely candidates include:
- `ik_output_buffer_create` - only OOM errors
- `ik_layer_cake_create` - only OOM errors
- Various other `_create` functions that only allocate

**Note:** Full list to be determined during exhaustive search when addressing this issue.

### Benefits

- Simpler, clearer code
- Fewer lines at call sites
- Less mental overhead (no false impression of error handling)
- Consistent with "direct pointer return" pattern already used elsewhere
- Better matches the actual semantics (allocation always succeeds or aborts)

---

## Issue: Inconsistent Callback Return Patterns

**Status:** Not yet addressed
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

---

## Issue: Refactor Borrowed Pointer Naming Convention

**Status:** Not yet addressed
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

---

## Future Issues

Additional issues and technical debt items will be added here as they are discovered.
