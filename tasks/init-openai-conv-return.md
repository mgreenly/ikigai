# Task: Fix ik_openai_conversation_create return type semantics

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
- src/openai/client.c

### Test patterns
- tests/unit/openai/client_*.c
- tests/integration/*_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_openai_conversation_create` returns `res_t` (current state)
- Function only ever PANICs on OOM, never returns ERR

## What
Change `ik_openai_conversation_create` to return pointer directly instead of `res_t`.

Per errors.md decision framework:
- OOM? -> PANIC (already done)
- Can happen with correct code? -> No, this is simple allocation
- Therefore: Return pointer directly, not res_t

## How

### Step 1: Identify all callers
Use sub-agent to grep for all usages of `ik_openai_conversation_create` across the codebase:
```bash
grep -rn "ik_openai_conversation_create" src/ tests/
```

### Step 2: Update function signature
In `src/openai/client.h`, change:
```c
res_t ik_openai_conversation_create(void *parent);
```
To:
```c
ik_openai_conversation_t *ik_openai_conversation_create(TALLOC_CTX *ctx);
```

Note: Also change `void *parent` to `TALLOC_CTX *ctx` per naming standardization.

### Step 3: Update function implementation
In `src/openai/client.c`, change:
```c
res_t ik_openai_conversation_create(void *parent) {
    ik_openai_conversation_t *conv = talloc_zero(parent, ik_openai_conversation_t);
    if (!conv) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate conversation"); // LCOV_EXCL_LINE
    }

    conv->messages = NULL;
    conv->message_count = 0;

    return OK(conv);
}
```
To:
```c
ik_openai_conversation_t *ik_openai_conversation_create(TALLOC_CTX *ctx) {
    ik_openai_conversation_t *conv = talloc_zero(ctx, ik_openai_conversation_t);
    if (!conv) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate conversation"); // LCOV_EXCL_LINE
    }

    conv->messages = NULL;
    conv->message_count = 0;

    return conv;
}
```

### Step 4: Update all callers
For each caller found in Step 1, change:
```c
res_t result = ik_openai_conversation_create(ctx);
if (is_err(&result)) { /* error handling */ }
ik_openai_conversation_t *conv = result.ok;
```
To:
```c
ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);
```

### Step 5: Verify
Run `make check` to ensure no compilation errors or test failures.

## Why
The current signature suggests the function can fail with recoverable errors, but it only ever PANICs on OOM. This is misleading to callers and adds unnecessary error handling boilerplate. Per the errors.md decision framework, simple allocation that only fails on OOM should return the pointer directly.

## TDD Cycle

### Red
N/A - existing tests serve as safety net for this refactoring.

### Green
Apply the signature change and update all callers.

### Refactor
Verify with `make check` that all tests pass.

## Post-conditions
- `make check` passes with no errors
- `ik_openai_conversation_create` returns `ik_openai_conversation_t *` directly
- Parameter is named `TALLOC_CTX *ctx` (not `void *parent`)
- All callers updated to not use res_t handling
- Working tree is clean (changes committed)

## Complexity
Low - mechanical signature change with caller updates

## Notes
- This function has many callers across unit and integration tests
- Use sub-agents to find all callers before making changes
- The similar function `ik_openai_msg_create_tool_call` already uses the correct pattern (returns pointer directly)
