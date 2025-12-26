---
name: memory
description: talloc-based memory management and ownership rules for ikigai
---

# Memory Management

talloc hierarchical memory allocator. Use for all new code.

## Ownership Rules

- Caller owns returned pointers (ownership transfer)
- Each allocation has exactly one owner who frees it
- Children freed with parent automatically
- Document ownership in function comments
- Fields must be children of their struct (not siblings)

## Core API

```c
TALLOC_CTX *talloc_new(const void *parent);
void *talloc(const void *ctx, type);
void *talloc_zero(const void *ctx, type);
void *talloc_array(const void *ctx, type, count);
char *talloc_strdup(const void *ctx, const char *str);
char *talloc_asprintf(const void *ctx, const char *fmt, ...);
int talloc_free(void *ptr);  // Frees ptr and ALL children
void *talloc_steal(const void *new_parent, const void *ptr);
```

## Pattern: Request Processing

```c
void handle_request(const char *input) {
    TALLOC_CTX *req_ctx = talloc_new(NULL);
    res_t res = ik_protocol_msg_parse(req_ctx, input);
    if (is_err(&res)) { talloc_free(req_ctx); return; }
    // ... process ...
    talloc_free(req_ctx);  // Frees all children
}
```

## Pattern: Allocate on Caller's Context

```c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path) {
    ik_cfg_t *config = talloc_zero_(ctx, sizeof(ik_cfg_t));
    if (!config) PANIC("Out of memory");
    config->key = talloc_strdup(config, key);  // Child of config
    return OK(config);
}
```

## Pattern: Fields as Children (CRITICAL)

```c
// CORRECT - fields are children of struct
foo->name = talloc_strdup(foo, "example");  // Child of foo

// WRONG - sibling causes leak
foo->name = talloc_strdup(ctx, "example");  // Sibling, NOT freed with foo
```

## Pattern: Temporary Contexts

```c
res_t process(TALLOC_CTX *ctx, input_t *in) {
    TALLOC_CTX *tmp = talloc_new(ctx);
    // intermediates on tmp...
    if (keep_result) talloc_steal(ctx, result);
    talloc_free(tmp);
    return OK(result);
}
```

## CRITICAL: Error Context Lifetime

**Never allocate errors on temporary contexts.**

```c
// WRONG - use-after-free
res_t res = some_function(tmp);  // Error on tmp
talloc_free(tmp);                // FREES THE ERROR
return res;                      // Crash

// CORRECT - pass parent context for errors
res_t res = some_function(ctx);  // Error on ctx (survives tmp free)
```

**Rule**: Functions that can fail allocate errors on parent context (first param).

## Function Naming

- `*_init(TALLOC_CTX *ctx, foo_t **out)` - Allocate on ctx, return via out
- `*_create()`, `*_load()`, `*_parse()` - Allocate and return owned pointer
- `*_free()` - Deallocate object and children

## When NOT to Use talloc

- FFI boundaries expecting `free()`-able memory
- Long-lived singletons/global state

## OOM Handling

```c
void *ptr = talloc_zero(ctx, type);
if (!ptr) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
```

## Debugging

```c
talloc_enable_leak_report_full();
talloc_report_full(context, stdout);
```

## Common Mistakes

1. Allocating fields on ctx instead of struct parent
2. Error on temp context (pass parent context instead)
3. `talloc_new(NULL)` outside main() (should receive parent)
4. Mixing malloc/free with talloc

---

See `project/memory.md` for full details.
