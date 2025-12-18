# Task: Fix ik_openai_msg_create return type semantics

## Target
Refactoring #4: Standardize Initialization Patterns (Return Type Semantics - Phase 1)

## Pre-read

### Skills
- scm
- di
- errors
- style
- tdd

### Source patterns
- src/openai/client.h
- src/openai/client_msg.c

### Test patterns
- tests/unit/openai/client_*.c
- tests/integration/*_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_openai_msg_create` returns `res_t` (current state)
- Function only ever PANICs on OOM, never returns ERR
- Task `init-openai-conv-return` has been completed

## What
Change `ik_openai_msg_create` to return pointer directly instead of `res_t`.

Per errors.md decision framework:
- OOM? -> PANIC (already done)
- Can happen with correct code? -> No, this is simple allocation
- Therefore: Return pointer directly, not res_t

Note: The sibling functions `ik_openai_msg_create_tool_call` and `ik_openai_msg_create_tool_result` already use the correct pattern.

## How

### Step 1: Identify all callers
Use sub-agent to grep for all usages of `ik_openai_msg_create` across the codebase:
```bash
grep -rn "ik_openai_msg_create\b" src/ tests/
```
Note: Use word boundary `\b` to avoid matching `ik_openai_msg_create_tool_call` and `ik_openai_msg_create_tool_result`.

### Step 2: Update function signature
In `src/openai/client.h`, change:
```c
res_t ik_openai_msg_create(void *parent, const char *role, const char *content);
```
To:
```c
ik_msg_t *ik_openai_msg_create(TALLOC_CTX *ctx, const char *role, const char *content);
```

Note: Also change `void *parent` to `TALLOC_CTX *ctx` per naming standardization.

### Step 3: Update function implementation
In `src/openai/client_msg.c`, change:
```c
res_t ik_openai_msg_create(void *parent, const char *role, const char *content) {
    assert(role != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate message"); // LCOV_EXCL_LINE
    }
    // ... field assignments ...
    return OK(msg);
}
```
To:
```c
ik_msg_t *ik_openai_msg_create(TALLOC_CTX *ctx, const char *role, const char *content) {
    assert(role != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL); // LCOV_EXCL_BR_LINE

    ik_msg_t *msg = talloc_zero(ctx, ik_msg_t);
    if (!msg) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate message"); // LCOV_EXCL_LINE
    }
    // ... field assignments ...
    return msg;
}
```

### Step 4: Update all callers
For each caller found in Step 1, change:
```c
res_t msg_res = ik_openai_msg_create(parent, "role", "content");
if (is_err(&msg_res)) { /* error handling */ }
ik_msg_t *msg = msg_res.ok;
```
To:
```c
ik_msg_t *msg = ik_openai_msg_create(ctx, "role", "content");
```

Special attention to `src/openai/client.c` where this pattern is used:
```c
res_t msg_res = ik_openai_msg_create(parent, "assistant", http_resp->content);
if (msg_res.is_err) {  // LCOV_EXCL_BR_LINE
    talloc_free(http_resp);  // LCOV_EXCL_LINE
    return msg_res;  // LCOV_EXCL_LINE
}
msg = msg_res.ok;
```
This should become:
```c
msg = ik_openai_msg_create(parent, "assistant", http_resp->content);
```

### Step 5: Verify
Run `make check` to ensure no compilation errors or test failures.

## Why
The current signature suggests the function can fail with recoverable errors, but it only ever PANICs on OOM. This is misleading to callers and adds unnecessary error handling boilerplate. Per the errors.md decision framework, simple allocation that only fails on OOM should return the pointer directly.

This creates consistency with sibling functions:
- `ik_openai_msg_create_tool_call` - already returns `ik_msg_t *`
- `ik_openai_msg_create_tool_result` - already returns `ik_msg_t *`

## TDD Cycle

### Red
N/A - existing tests serve as safety net for this refactoring.

### Green
Apply the signature change and update all callers.

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `ik_openai_msg_create` returns `ik_msg_t *` directly
- Parameter is named `TALLOC_CTX *ctx` (not `void *parent`)
- All callers updated to not use res_t handling
- Working tree is clean (changes committed)

## Complexity
Medium - signature change with many callers, some with error handling to remove

## Notes
- This function is called from many places including client.c and numerous tests
- The error handling removal in client.c simplifies the code significantly
- Use sub-agents to find all callers before making changes
