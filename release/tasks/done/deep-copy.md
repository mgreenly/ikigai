# Task: Update Deep Copy for Thinking Signatures

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** types.md, content-block-api.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/providers/request_tools.c` - Deep copy (lines 179-217)
- `src/agent_messages.c` - Clone content block (lines 47-74)
- `tests/unit/providers/request_tools_copy_test.c` - Test patterns

**Plan:**
- `release/plan/thinking-signatures.md` - Section 8

## Libraries

- `talloc` - Already used for memory management

Do not introduce new dependencies.

## Preconditions

- [ ] Git workspace is clean
- [ ] `types.md` task completed (signature field and redacted_thinking type exist)

## Objective

Update all content block copy functions to:
1. Copy `signature` field for thinking blocks
2. Handle `IK_CONTENT_REDACTED_THINKING` blocks

## Interface

### File: `src/providers/request_tools.c`

**Function:** `ik_request_add_message_direct` (static, lines 164-229)

Update thinking block copy (lines 209-212):

Current:
```c
case IK_CONTENT_THINKING:
    dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
    if (dst->data.thinking.text == NULL) PANIC("Out of memory");
    break;
```

New:
```c
case IK_CONTENT_THINKING:
    dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
    if (dst->data.thinking.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (src->data.thinking.signature != NULL) {
        dst->data.thinking.signature = talloc_strdup(copy, src->data.thinking.signature);
        if (dst->data.thinking.signature == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        dst->data.thinking.signature = NULL;
    }
    break;
```

Add new case before `default`:
```c
case IK_CONTENT_REDACTED_THINKING:
    dst->data.redacted_thinking.data = talloc_strdup(copy, src->data.redacted_thinking.data);
    if (dst->data.redacted_thinking.data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    break;
```

### File: `src/agent_messages.c`

**Function:** `clone_content_block` (static, lines 47-74)

Update thinking block handling (lines 69-73):

Current:
```c
} else {
    // IK_CONTENT_THINKING (only remaining type)
    dest_block->data.thinking.text = talloc_strdup(ctx, src_block->data.thinking.text);
    if (!dest_block->data.thinking.text) PANIC("Out of memory");
}
```

New:
```c
} else if (src_block->type == IK_CONTENT_THINKING) {
    dest_block->data.thinking.text = talloc_strdup(ctx, src_block->data.thinking.text);
    if (!dest_block->data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (src_block->data.thinking.signature != NULL) {
        dest_block->data.thinking.signature = talloc_strdup(ctx, src_block->data.thinking.signature);
        if (!dest_block->data.thinking.signature) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        dest_block->data.thinking.signature = NULL;
    }
} else {
    // IK_CONTENT_REDACTED_THINKING
    dest_block->data.redacted_thinking.data = talloc_strdup(ctx, src_block->data.redacted_thinking.data);
    if (!dest_block->data.redacted_thinking.data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
}
```

## Behaviors

- Signature is copied only if non-NULL in source
- If source signature is NULL, destination signature is set to NULL explicitly
- Redacted thinking data is always non-NULL (required field)
- PANIC on OOM with LCOV exclusion markers

## Test Scenarios

Update `tests/unit/providers/request_tools_copy_test.c`:

1. `test_copy_thinking_with_signature` - Verify signature copied
2. `test_copy_thinking_null_signature` - Verify NULL signature handled
3. `test_copy_redacted_thinking` - Verify redacted data copied

Update or add tests in `tests/unit/agent/messages_test.c`:

1. `test_clone_thinking_with_signature` - Verify signature cloned
2. `test_clone_redacted_thinking` - Verify redacted block cloned

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(deep-copy.md): success - update deep copy for thinking signatures

Updated request_tools.c and agent_messages.c to copy signature
field for thinking blocks and handle redacted_thinking blocks.
EOF
)"
```

Report status: `/task-done deep-copy.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
