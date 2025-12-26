---
name: refactoring/memory
description: Memory Refactoring (talloc) refactoring skill
---

# Memory Refactoring (talloc)

talloc-specific refactoring patterns. Focus on ownership, hierarchy, and error context lifetime.

## Pattern 1: Fix Error Context Lifetime

**Smell:** Error allocated on context that gets freed before error is returned.

```c
// BROKEN - use-after-free
res_t ik_foo_init(void *parent, foo_t **out) {
    foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
    res_t result = ik_bar_init(foo, &foo->bar);  // Error on foo
    if (is_err(&result)) {
        talloc_free(foo);  // FREES THE ERROR!
        return result;     // Crash
    }
}
```

**Fix A (Preferred):** Allocate on parent first, then steal:
```c
bar_t *bar = NULL;
res_t result = ik_bar_init(parent, &bar);  // Error survives
if (is_err(&result)) return result;
foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
talloc_steal(foo, bar);
foo->bar = bar;
```

**Fix B:** Reparent error before freeing:
```c
if (is_err(&result)) {
    talloc_steal(parent, result.err);  // Save error
    talloc_free(foo);
    return result;
}
```

## Pattern 2: Introduce Temp Context

**Smell:** Intermediate allocations leak or complicate cleanup.

```c
// AFTER - temp context for intermediates
res_t process(void *ctx, input_t *in) {
    void *tmp = talloc_new(ctx);
    char *buf1 = talloc_array(tmp, char, 1024);
    // ... work ...
    if (keep_result) talloc_steal(ctx, result);
    talloc_free(tmp);  // Clean all intermediates
    return OK(result);
}
```

**Critical:** Errors must be on `ctx`, not `tmp`. Pass `ctx` to functions that might fail.

## Pattern 3: Add Missing Context Parameter

**Smell:** Function allocates internally without context parameter.

```c
// BEFORE - hidden allocation
char *format_message(const char *fmt, ...) {
    char *buf = malloc(1024);  // Who frees?
    return buf;
}

// AFTER - caller owns via ctx
char *ik_format_msg(void *ctx, const char *fmt, ...) {
    return talloc_array(ctx, char, 1024);
}
```

## Pattern 4: Fix Orphaned Allocations

**Smell:** `talloc_new(NULL)` in non-main function.

**Fix:** Receive parent from caller:
```c
res_t ik_module_init(void *parent, module_t **out) {
    module_t *mod = talloc_zero_(parent, sizeof(module_t));
    // mod freed when parent freed
}
```

## Pattern 5: Convert malloc to talloc

```c
// BEFORE
char *buf = malloc(size);
free(buf);

// AFTER
char *buf = talloc_array(ctx, char, size);
// Freed automatically with ctx
```

## Pattern 6: Proper Child Relationships

**Smell:** Struct fields allocated on wrong context (as siblings).

```c
// WRONG - fields are siblings
foo->name = talloc_strdup(ctx, name);

// CORRECT - fields are children of struct
foo->name = talloc_strdup(foo, name);
foo->data = talloc_array(foo, char, size);
// talloc_free(foo) frees everything
```

## Refactoring Checklist

- `malloc(` → Convert to talloc
- `talloc_new(NULL)` → Should only be in main()
- `talloc_free` after `is_err` → Check error context lifetime
- `_init` functions → Ensure ctx param, proper child allocation
- Struct field allocation → Fields should be children of struct
- Complex cleanup → Simplify with temp context
