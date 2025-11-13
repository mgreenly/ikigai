# Memory + Error Handling Integration

## The Core Pattern

Our `res_t` error system and talloc hierarchical memory work together to solve a critical problem: **errors must outlive the context that failed**.

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
return ERR(parent_ctx, OOM, "Failed to allocate");

// ❌ WRONG: Error allocated on temporary context (would be freed!)
return ERR(tmp_ctx, OOM, "Failed to allocate");
```

## How It Works in Practice

### Example from `src/array.c:45-69`

```c
static res_t grow_array(ik_array_t *array)
{
    TALLOC_CTX *ctx = talloc_parent(array);  // Get parent context

    // Try to reallocate
    void *new_data = ik_talloc_realloc_wrapper(ctx, array->data, ...);
    if (!new_data) {
        // Allocate error on PARENT context (ctx)
        return ERR(ctx, OOM, "Failed to grow array capacity");
    }

    array->data = new_data;
    return OK(NULL);
}
```

**Why this works:**
1. Caller owns `array`, allocated on some context
2. Function gets parent via `talloc_parent(array)`
3. Error allocated on **that parent context**
4. Even if `array` gets freed on error path, error survives
5. Caller can examine error, log it, propagate it

### Example from `src/workspace.c:13-38`

```c
res_t ik_workspace_create(void *parent, ik_workspace_t **workspace_out)
{
    workspace_t *workspace = ik_talloc_zero_wrapper(parent, ...);

    res_t res = ik_byte_array_create(workspace, 64);
    if (is_err(&res)) {
        talloc_free(workspace);  // Frees workspace and children
        return res;              // Error still alive on parent!
    }

    res = ik_cursor_create(workspace, &workspace->cursor);
    if (is_err(&res)) {
        talloc_free(workspace);  // Frees workspace, byte_array, and children
        return res;              // Error still alive on parent!
    }

    return OK(workspace);
}
```

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

thing_t *thing = talloc(ctx, thing_t);
if (!init(thing)) {
    talloc_free(thing);  // Free just this allocation
    // Error is on ctx, survives the free
    return ERR(ctx, INIT_FAILED, "Initialization failed");
}
```

## Key Insights

1. **Errors outlive failures** - Allocated on parent, survive child cleanup
2. **Granular lifetime control** - Free at any level of hierarchy
3. **No manual tracking** - Parent-child relationships handle cleanup
4. **Uniform memory model** - Errors and data use same allocator

## See Also

- `docs/memory.md` - Full memory management guide
- `docs/error_handling.md` - Error handling patterns
- `src/error.h:97-106` - Error allocation implementation
