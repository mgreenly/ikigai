# Analysis: ik_db_session_create Parameter Order

## Date
2025-12-18

## Task
init-db-session-param-order.md

## Question
Does `ik_db_session_create` violate DI principles by having `db_ctx` as first parameter instead of `TALLOC_CTX`?

## Current Signature
```c
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```

## Analysis

### DI Principle Under Review
Per `.agents/skills/di.md` Rule 2: "First param is TALLOC_CTX - Explicit memory ownership"

This rule applies when:
- The function allocates objects owned by the caller
- The caller needs to specify memory ownership

### Function Behavior
Looking at `src/db/session.c`:
1. Creates temporary talloc context internally: `talloc_new(NULL)` (line 23)
2. Executes SQL INSERT query
3. Extracts session_id primitive from result
4. Cleans up temporary context
5. Returns session_id via output parameter (line 50)

### Key Observations
1. **No Caller-Owned Allocations**: The function returns a primitive (int64_t), not an allocated object
2. **Internal Memory Management**: Temporary context is created and freed within function scope
3. **Error Allocation**: Errors are allocated on `db_ctx` (line 36: `ERR(db_ctx, IO, ...)`)
4. **Error Lifetime**: Per `.agents/skills/errors.md`, error allocation context must survive the error's return - `db_ctx` satisfies this requirement

### Comparison with DI-Compliant Functions
Functions that DO need TALLOC_CTX as first parameter:
```c
// Allocates config object for caller
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path, ik_cfg_t **out_cfg);

// Allocates database context for caller
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_db);
```

Functions like `ik_db_session_create` that return primitives:
```c
// Returns primitive - no allocation needed
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_end(ik_db_ctx_t *db_ctx, int64_t session_id);
```

## Decision
**NO CHANGE NEEDED**

The current signature is correct:
1. The function returns a primitive, not an allocated object
2. No memory ownership needs to be specified by the caller
3. The `db_ctx` is the appropriate first parameter for database operations
4. Error allocation uses `db_ctx`, which correctly survives error return

Adding `TALLOC_CTX *ctx` as first parameter would be:
- Unnecessary (no caller-owned allocations)
- Inconsistent (primitives don't need memory context)
- Confusing (what would the ctx be used for?)

## Related Functions
The same analysis applies to:
- `ik_db_session_get_active` - returns primitive (int64_t)
- `ik_db_session_end` - takes primitive, no allocation

All three functions correctly use `db_ctx` as first parameter.

## Verification
- All tests pass: `make check` ✓
- No code changes required
- Working tree remains clean

## References
- Task: `rel-06/tasks/init-db-session-param-order.md`
- DI principles: `.agents/skills/di.md`
- Error allocation rules: `.agents/skills/errors.md`
- Implementation: `src/db/session.c`
- Header: `src/db/session.h`
