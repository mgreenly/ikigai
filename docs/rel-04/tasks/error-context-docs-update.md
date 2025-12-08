# Task: Document Error Context Lifetime Rules

## Target
Documentation: Prevent future UAF bugs by documenting error allocation context rules

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/errors.md

### Pre-read Docs
- fix.md (the UAF bug pattern analysis)
- docs/error_handling.md (current documentation)
- docs/return_values.md (current documentation)
- docs/memory.md (current documentation - Pattern 4 is misleading)
- docs/error_patterns.md (current documentation)

## Pre-conditions
- `make check` passes
- UAF bug pattern documented in fix.md

## Task
Update documentation to warn about the error context lifetime trap and reinforce the correct pattern.

The core rule to document:

> **Error Allocation Context Rule:** When returning an error after freeing a context, the error must be allocated on a context that survives the free. Either:
> 1. Allocate errors on the caller's parent context (passed as first parameter)
> 2. Use `talloc_steal(survivor, result.err)` before freeing if error is on doomed context

### Files to Update

#### 1. `docs/error_handling.md`

Add new section after "Result Types - Memory Management" (around line 254):

```markdown
### Error Context Lifetime (Critical)

**WARNING:** Errors allocated on a context that gets freed become use-after-free bugs.

**The Trap:**
```c
res_t ik_foo_init(void *parent, foo_t **out)
{
    foo_t *foo = talloc_zero_(parent, sizeof(foo_t));

    res_t result = ik_bar_init(foo, &foo->bar);  // Error allocated on foo
    if (is_err(&result)) {
        talloc_free(foo);  // FREES THE ERROR TOO!
        return result;     // USE-AFTER-FREE
    }
    // ...
}
```

**The Fix - Option A (Preferred):** Pass parent to sub-functions:
```c
res_t result = ik_bar_init(parent, &bar);  // Error on parent
if (is_err(&result)) {
    return result;  // Error survives - it's on parent
}
// After all failable work succeeds:
foo_t *foo = talloc_zero_(parent, sizeof(foo_t));
talloc_steal(foo, bar);
foo->bar = bar;
```

**The Fix - Option B:** Reparent error before freeing:
```c
if (is_err(&result)) {
    talloc_steal(parent, result.err);  // Save error
    talloc_free(foo);
    return result;
}
```

**Rule:** Error allocation context must be a parent/sibling of any context freed on error path.

See `fix.md` for detailed analysis of this bug pattern.
```

#### 2. `docs/memory.md`

Update Pattern 4 (lines 267-288) to warn about the trap:

```markdown
### Pattern 4: Using temporary contexts for intermediate work

**WARNING:** This pattern has a UAF trap - see note at end.

```c
res_t ik_openai_stream_req(TALLOC_CTX *ctx, ...) {
    TALLOC_CTX *tmp = talloc_new(ctx);

    // All intermediate allocations on tmp
    sse_parser_t *parser = talloc_zero(tmp, sse_parser_t);
    // ... perform request ...

    // Result allocated on ctx, NOT tmp
    res_t result;
    if (error_occurred) {
        result = ERR(ctx, NETWORK, "Request failed");  // ctx, NOT tmp!
    } else {
        result = OK(NULL);
    }

    talloc_free(tmp);
    return result;
}
```

**Critical:** If `ERR(tmp, ...)` were used instead of `ERR(ctx, ...)`, freeing `tmp` would free the error, causing use-after-free when caller accesses `result.err`.

**Safe patterns:**
- Allocate errors on `ctx` (the caller's context)
- Or use `talloc_steal(ctx, result.err)` before `talloc_free(tmp)`

See `fix.md` for detailed analysis.
```

#### 3. `docs/return_values.md`

Add clarification to line 37 area:

```markdown
**Talloc-allocated** - Errors are allocated on the parent context. The error remains valid as long as that context exists. **WARNING:** If you free the context the error is allocated on, the error becomes invalid. See `docs/error_handling.md#error-context-lifetime-critical`.
```

#### 4. `.agents/skills/errors.md`

Add to the skill file (which you already updated based on git status):

```markdown
## Error Context Lifetime

**Critical rule:** Error allocation context must survive any cleanup before return.

**The trap:**
```c
thing *ctx = talloc_zero_(parent, ...);
res_t result = some_function(ctx, ...);  // error on ctx
if (is_err(&result)) {
    talloc_free(ctx);  // FREES THE ERROR
    return result;     // USE AFTER FREE
}
```

**Fix:** Either pass `parent` to sub-functions, or `talloc_steal(parent, result.err)` before freeing.

See `fix.md` for full analysis.
```

## TDD Cycle

### Red
N/A - Documentation only task

### Green
1. Update `docs/error_handling.md` with new "Error Context Lifetime" section
2. Update `docs/memory.md` Pattern 4 with warning
3. Update `docs/return_values.md` with clarification
4. Update `.agents/skills/errors.md` with the rule

### Refactor
1. Review changes for clarity and consistency
2. Ensure cross-references work (`fix.md` exists)
3. No code changes needed

## Post-conditions
- All four documentation files updated
- New "Error Context Lifetime" section in error_handling.md
- Pattern 4 in memory.md has warning
- return_values.md references the critical section
- errors.md skill has the rule
- No code changes (documentation only)
