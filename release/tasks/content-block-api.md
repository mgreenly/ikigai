# Task: Update Content Block Builder API

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions
- `/load naming` - For function naming conventions

**Source:**
- `src/providers/request.h` - Function declarations (lines 57-64)
- `src/providers/request.c` - Function implementations (lines 82-93)
- `tests/unit/providers/request_test.c` - Test patterns

**Plan:**
- `release/plan/thinking-signatures.md` - Section 4

## Libraries

- `talloc` - Already used for memory management

Do not introduce new dependencies.

## Preconditions

- [ ] Git workspace is clean
- [ ] `types.md` task completed (signature field and redacted_thinking type exist)

## Objective

1. Update `ik_content_block_thinking` to accept signature parameter
2. Add new `ik_content_block_redacted_thinking` function

## Interface

### Modified Function

**Declaration (request.h):**

```c
/**
 * Create thinking content block with signature
 *
 * @param ctx       Talloc parent context
 * @param text      Thinking text (will be copied)
 * @param signature Cryptographic signature (will be copied, may be NULL)
 * @return          Allocated content block
 */
ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx,
                                               const char *text,
                                               const char *signature);
```

**Implementation (request.c):**
- Add `const char *signature` parameter
- Add assertion: `assert(text != NULL)` (signature can be NULL for backwards compat)
- Copy signature with `talloc_strdup` if non-NULL, else set to NULL

### New Function

**Declaration (request.h):**

```c
/**
 * Create redacted thinking content block
 *
 * @param ctx  Talloc parent context
 * @param data Encrypted opaque data (will be copied)
 * @return     Allocated content block
 */
ik_content_block_t *ik_content_block_redacted_thinking(TALLOC_CTX *ctx,
                                                        const char *data);
```

**Implementation (request.c):**
- Assert `data != NULL`
- Allocate block with `talloc_zero`
- Set `block->type = IK_CONTENT_REDACTED_THINKING`
- Copy data with `talloc_strdup`
- PANIC on OOM with `// LCOV_EXCL_BR_LINE` markers

## Behaviors

- `ik_content_block_thinking`: signature parameter can be NULL (for testing/backwards compat)
- `ik_content_block_redacted_thinking`: data parameter must not be NULL
- Both functions PANIC on OOM (consistent with existing builders)

## Test Scenarios

Update `tests/unit/providers/request_test.c`:

1. `test_content_block_thinking_with_signature` - Verify signature stored
2. `test_content_block_thinking_null_signature` - Verify NULL signature handled
3. `test_content_block_redacted_thinking` - Verify redacted block creation

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(content-block-api.md): success - update content block builder API

Added signature parameter to ik_content_block_thinking.
Added ik_content_block_redacted_thinking function.
Updated tests for new API.
EOF
)"
```

Report status: `/task-done content-block-api.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
