# Task: Add Pending Thinking Fields to Agent Context

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** None (Phase 1 complete)

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/agent.h` - Agent context struct (lines 100-130)
- `src/agent.c` - Agent creation and cleanup (lines 70-140, 200-270)

**Plan:**
- `release/plan/thinking-signatures.md` - Section 9 (Agent Context Changes)

## Libraries

No new dependencies.

## Preconditions

- [ ] Git workspace is clean (verify with `git status --porcelain`)

## Objective

Add fields to `ik_agent_ctx_t` to store pending thinking blocks from API response, for use when constructing tool call messages.

## Interface

### Struct Changes

**File:** `src/agent.h`

Add after `pending_tool_call` field (around line 121):

```c
// Pending thinking blocks from response (for tool call messages)
char *pending_thinking_text;
char *pending_thinking_signature;
char *pending_redacted_data;
```

### Initialization

**File:** `src/agent.c`

In `ik_agent_create` (after line 134), initialize to NULL:

```c
agent->pending_thinking_text = NULL;
agent->pending_thinking_signature = NULL;
agent->pending_redacted_data = NULL;
```

**Note:** `ik_agent_restore` (in `src/repl/agent_restore.c`) does NOT need changes - it does not create new agent structs. The pending thinking fields are only relevant for live agents receiving API responses.

### Cleanup

Memory is automatically freed via talloc when agent is freed.

## Behaviors

- Fields are NULL when no thinking block is pending
- Fields are set when response contains thinking + tool_call
- Fields are cleared after tool call message is created (handled by later task)

## Test Scenarios

No new tests in this task. The fields are data-only changes covered by subsequent tasks.

Verify compilation:
```bash
make clean && make
```

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(agent-context.md): success - add pending thinking fields

Added pending_thinking_text, pending_thinking_signature, and
pending_redacted_data fields to ik_agent_ctx_t for tool call
message construction.
EOF
)"
```

Report status: `/task-done agent-context.md`

## Postconditions

- [ ] Compiles without warnings (`make`)
- [ ] All existing tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
