# Task: Add Thinking Signature Types

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For understanding PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/providers/provider.h` - The file to modify (read lines 55-165)

**Plan:**
- `release/plan/thinking-signatures.md` - Sections 1, 2

## Libraries

No new dependencies. This is a pure header modification.

## Preconditions

- [ ] Git workspace is clean (verify with `git status --porcelain`)

## Objective

Add the `signature` field to the thinking content block struct and add the new `IK_CONTENT_REDACTED_THINKING` enum value.

## Interface

### Enum Change

Add `IK_CONTENT_REDACTED_THINKING = 4` after `IK_CONTENT_THINKING = 3` in `ik_content_type_t`.

### Struct Changes

1. Add `char *signature` field to the thinking struct (after `text`)
2. Add new union member for redacted_thinking:

```c
/* IK_CONTENT_REDACTED_THINKING */
struct {
    char *data; /* Encrypted opaque data (base64) */
} redacted_thinking;
```

## Behaviors

- These are data structure changes only
- No behavioral logic changes in this task
- Existing code using thinking blocks will still compile (signature will be NULL/uninitialized)

## Test Scenarios

No new tests in this task. The type changes are covered by subsequent tasks.

Verify compilation:
```bash
make clean && make
```

## Completion

After completing work:

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(types.md): success - add signature field and redacted_thinking type

Added char *signature to thinking struct in ik_content_block_t.
Added IK_CONTENT_REDACTED_THINKING enum value and redacted_thinking struct.
EOF
)"
```

Report status: `/task-done types.md`

## Postconditions

- [ ] Compiles without warnings (`make`)
- [ ] All existing tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
