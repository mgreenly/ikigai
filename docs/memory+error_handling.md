# Memory + Error Handling Integration

## The Core Pattern

Our `res_t` error system and talloc hierarchical memory work together to solve a critical problem: **errors must outlive the context that failed**.

**Note:** Out of memory conditions no longer return errors - they call `PANIC("Out of memory")` which immediately terminates the process. This document describes the interaction for non-OOM errors.

## Why This Matters

```c
res_t create_thing(TALLOC_CTX *parent) {
    // Create temporary context for construction
    TALLOC_CTX *tmp = talloc_new(parent);

    thing_t *thing = talloc(tmp, thing_t);

    res_t res = init_component(tmp, thing);
    if (is_err(&res)) {
        // ERROR is allocated on PARENT, not tmp
        talloc_free(tmp);  // Destroys thing and tmp
        return res;        // Error survives! Still on parent context
    }

    // Success - move to parent
    talloc_steal(parent, thing);
    talloc_free(tmp);
    return OK(thing);
}
```

## The Rule

**Always allocate errors on the parent context:**

```c
// ✅ CORRECT: Error allocated on caller's context
return ERR(parent_ctx, IO, "Failed to open file");

// ❌ WRONG: Error allocated on temporary context (would be freed!)
return ERR(tmp_ctx, IO, "Failed to open file");

// ✅ OOM: Use PANIC instead of ERR
void *ptr = talloc_zero_(ctx, size);
if (!ptr) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
```

## How It Works in Practice

### Example from `src/array.c:45-69`

```c
static res_t grow_array(ik_array_t *array)
{
    TALLOC_CTX *ctx = talloc_parent(array);  // Get parent context

    // Try to reallocate
    void *new_data = talloc_realloc_(ctx, array->data, ...);
    if (!new_data) {
        // OOM: PANIC immediately - cannot continue
        PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    array->data = new_data;
    return OK(NULL);
}
```

**Why PANIC for OOM:**
1. Memory allocation failure is unrecoverable
2. Cannot allocate error structure to return
3. Process cannot continue without memory
4. PANIC provides immediate termination with diagnostic output

### Example from `src/input_buffer.c:13-38`

```c
res_t ik_input_buffer_create(void *parent, ik_input_buffer_t **input_buffer_out)
{
    input_buffer_t *input_buffer = talloc_zero_(parent, sizeof(input_buffer_t));
    if (!input_buffer) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_byte_array_create(input_buffer, 64);
    if (is_err(&res)) {
        talloc_free(input_buffer);  // Frees input buffer and children
        return res;              // Error still alive on parent!
    }

    res = ik_cursor_create(input_buffer, &input_buffer->cursor);
    if (is_err(&res)) {
        talloc_free(input_buffer);  // Frees input buffer, byte_array, and children
        return res;              // Error still alive on parent!
    }

    return OK(input_buffer);
}
```

**Note:** The `ik_byte_array_create()` and `ik_cursor_create()` functions handle their own OOM conditions internally via PANIC. Only non-OOM errors (like invalid arguments) are returned as `res_t`.

## The Advantage Over Simple Pools

**Pool allocator (all-or-nothing):**
```c
pool_t *pool = pool_create();

thing_t *thing = pool_alloc(pool, sizeof(thing_t));
if (!init(thing)) {
    // Can't free just 'thing' - must destroy entire pool
    // If we destroy pool, we can't allocate error message on it!
    pool_destroy(pool);
    return strdup("Error");  // Must use malloc! Different system!
}
```

**Talloc hierarchy (granular cleanup):**
```c
TALLOC_CTX *ctx = talloc_new(NULL);
if (!ctx) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

thing_t *thing = talloc_zero_(ctx, sizeof(thing_t));
if (!thing) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

if (!init(thing)) {
    talloc_free(thing);  // Free just this allocation
    // Error is on ctx, survives the free
    return ERR(ctx, INIT_FAILED, "Initialization failed");
}
```

## Key Insights

1. **Errors outlive failures** - Allocated on parent, survive child cleanup (for non-OOM errors)
2. **Granular lifetime control** - Free at any level of hierarchy
3. **No manual tracking** - Parent-child relationships handle cleanup
4. **Uniform memory model** - Errors and data use same allocator
5. **OOM is unrecoverable** - Memory allocation failures cause PANIC (abort), not error returns

## See Also

- `docs/memory.md` - Full memory management guide
- `docs/error_handling.md` - Error handling patterns
- `src/error.h:97-106` - Error allocation implementation
