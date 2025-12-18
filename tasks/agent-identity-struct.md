# Task: Create ik_agent_identity_t Struct and Factory

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Identity Sub-context

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/ddd.md
- .agents/skills/di.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md

## Pre-read Source (patterns)

- src/agent.h (lines 54-120 - current ik_agent_ctx_t definition)
- src/agent.c (lines 32-141 - ik_agent_create implementation)
- src/shared.h (example of extracted context struct)

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (agent creation and field tests)
- tests/unit/agent/agent_restore_test.c (restore patterns)
- tests/unit/agent/meson.build (test registration pattern)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Current `ik_agent_ctx_t` has identity fields inline (uuid, name, parent_uuid, created_at, fork_message_id)

## Task

Extract identity fields from `ik_agent_ctx_t` into a new `ik_agent_identity_t` struct with its own factory function.

### What

Create a new struct `ik_agent_identity_t` containing:
- `char *uuid` - Internal unique identifier (22-char base64url)
- `char *name` - Optional human-friendly name (NULL if unnamed)
- `char *parent_uuid` - Parent agent's UUID (NULL for root agent)
- `int64_t created_at` - Unix timestamp when agent was created
- `int64_t fork_message_id` - Message ID at which this agent forked (0 for root)

### How

1. In `src/agent.h`:
   - Add `ik_agent_identity_t` struct definition BEFORE `ik_agent_ctx_t`
   - Add factory declaration: `res_t ik_agent_identity_create(TALLOC_CTX *ctx, const char *parent_uuid, ik_agent_identity_t **out);`
   - Add restore declaration: `res_t ik_agent_identity_restore(TALLOC_CTX *ctx, const char *uuid, const char *name, const char *parent_uuid, int64_t created_at, int64_t fork_message_id, ik_agent_identity_t **out);`
   - Keep the identity fields in `ik_agent_ctx_t` for now (will be replaced in compose task)

2. In `src/agent.c` (or new `src/agent_identity.c`):
   - Implement `ik_agent_identity_create()`:
     - Allocate `ik_agent_identity_t` under ctx
     - Generate UUID via `ik_generate_uuid()`
     - Set name to NULL
     - Copy parent_uuid if provided
     - Set created_at to current time
     - Set fork_message_id to 0
   - Implement `ik_agent_identity_restore()`:
     - Allocate `ik_agent_identity_t` under ctx
     - Copy all provided values (no generation)

### Why

The identity fields (5 total) represent a distinct concern: who this agent is. They are:
- Set once at creation or restore
- Rarely accessed together with display or LLM state
- Natural unit for persistence and comparison

Extracting them enables:
- Focused testing of identity creation
- Clearer ownership semantics
- Preparation for slim composite `ik_agent_ctx_t`

## TDD Cycle

### Red

Create `tests/unit/agent/agent_identity_test.c`:

1. Test `ik_agent_identity_create()` succeeds
2. Test identity->uuid is non-NULL and 22 chars
3. Test identity->uuid contains only base64url characters
4. Test identity->name is NULL initially
5. Test identity->parent_uuid is NULL when NULL passed
6. Test identity->parent_uuid matches input when provided
7. Test identity->created_at is set to current time (within 1 second tolerance)
8. Test identity->fork_message_id is 0 initially
9. Test identity is allocated under provided parent context
10. Test `ik_agent_identity_restore()` sets all fields from parameters

Run `make check` - expect compilation failure (struct doesn't exist yet)

### Green

1. Add struct definition to `src/agent.h`
2. Add factory declarations to `src/agent.h`
3. Implement factories in `src/agent.c`
4. Add test file to `tests/unit/agent/meson.build`
5. Run `make check` - expect pass

### Refactor

1. Verify naming follows `ik_` prefix convention
2. Verify factory follows `*_create()` pattern
3. Verify struct fields match existing `ik_agent_ctx_t` identity fields exactly
4. Run `make lint` - verify clean

## Post-conditions

- `make check` passes
- `make lint` passes
- `ik_agent_identity_t` struct exists in `src/agent.h`
- `ik_agent_identity_create()` and `ik_agent_identity_restore()` implemented
- Unit tests cover all identity fields
- Working tree is clean (all changes committed)

## Sub-agent Usage

- Use sub-agents to verify existing usages of `agent->uuid`, `agent->name`, etc. before proceeding to migration
- Pattern: `grep -r "agent->uuid" src/` to find all callers

## Notes

This is the first of several extraction tasks. The struct is created standalone first, then composed into `ik_agent_ctx_t` in a later task. This allows incremental testing and validation.
