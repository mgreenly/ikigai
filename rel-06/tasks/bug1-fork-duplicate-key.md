# Task: Fix /fork duplicate key constraint violation after /kill

## Target
Bug 1 from gap.md: `/fork` fails with duplicate key constraint after `/kill`

## Macro Context

**What:** After killing a child agent and then running `/fork`, the database insert fails because the child's UUID matches an existing UUID (the root agent's).

**Why this matters:**
- This breaks the core workflow of creating and managing multiple agents
- Users cannot fork new agents after killing existing ones
- The UUID generation system appears to have a collision issue

**How the bug manifests:**
```
Error: Failed to insert agent: ERROR:  duplicate key value violates unique constraint "agents_pkey"
DETAIL:  Key (uuid)=(wlT4G-jnTXaaLmMzn8maZg) already exists.
```

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

## Pre-read Docs
- rel-06/gap.md (Bug 1 section - investigation details)

## Pre-read Source
- src/uuid.c (READ - `ik_generate_uuid()` uses rand() - **CRITICAL: srand() is NOT called in production code**)
- src/agent.c (READ - `ik_agent_create()` at line 42 calls `ik_generate_uuid()`)
- src/commands_agent.c (READ - `cmd_fork()` at line 489 creates child agent)
- src/db/agent.c (READ - `ik_db_agent_insert()` uses agent->uuid for INSERT)

## Source Files to MODIFY
- src/client.c (likely - add srand() initialization)
- src/uuid.c (possibly - if rand() needs to be replaced)

## Investigation Context

**Key Finding from gap.md:**
> The error suggests `child->uuid` somehow equals parent UUID after creation

**Hypothesis 1: Missing srand() initialization**
- `ik_generate_uuid()` uses `rand()` which requires `srand()` for random seeding
- Grep shows `srand()` is ONLY called in tests (`tests/unit/uuid_test.c`, `tests/unit/agent/agent_test.c`)
- **Production code never calls srand()!**
- Without `srand()`, `rand()` starts with seed 1 and produces the same sequence every time
- If the process was restarted at some point between creating root and forking, the UUID sequence could repeat

**Hypothesis 2: Deterministic rand() across restarts**
- Even with `srand(time(NULL))`, if two startups happen in the same second, UUIDs collide
- Consider using more entropy sources (pid, microseconds, /dev/urandom)

**Code Flow Analysis:**
1. `cmd_fork()` calls `ik_agent_create(repl, repl->shared, parent->uuid, &child)`
2. `ik_agent_create()` line 42: `agent->uuid = ik_generate_uuid(agent)`
3. `ik_generate_uuid()` line 19: `bytes[i] = (unsigned char)(rand() & 0xFF)`
4. `cmd_fork()` line 508: `ik_db_agent_insert(repl->shared->db_ctx, child)`
5. `ik_db_agent_insert()` uses `child->uuid` for INSERT

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Gap 0 and Gap 1 tasks completed

## Task

### Part 1: Verify the root cause
1. Add temporary debug logging to confirm UUID values at each step
2. Verify that `srand()` is indeed missing from production startup code
3. Reproduce the bug scenario

### Part 2: Fix UUID generation initialization
1. Add `srand()` call in `src/client.c` during application startup
2. Use high-quality seed: `srand((unsigned int)(time(NULL) ^ getpid()))`
3. Alternatively, consider using `/dev/urandom` for better randomness

### Part 3: Consider alternative UUID generation
- Option A: Keep rand() with proper seeding (simplest fix)
- Option B: Use `/dev/urandom` for cryptographic randomness (more robust)
- Option C: Use UUID library like libuuid (overkill for this use case)

**Recommendation:** Start with Option A, document any edge cases.

### Part 4: Add test coverage
1. Create a test that verifies unique UUIDs are generated across multiple calls
2. Test UUID uniqueness even without explicit srand() (to prevent regression)

## TDD Cycle

### Red
1. Create a test that reproduces the bug scenario (kill then fork)
2. Run `make check` - test should fail showing duplicate key error

### Green
1. Add srand() initialization in client.c
2. Run the test - should pass
3. Run `make check` - all tests pass
4. Run `make lint` - passes

### Refactor
1. Consider if UUID generation should be more robust
2. Add comments explaining the seeding strategy
3. Ensure any test helpers also have proper seeding

## Sub-agent Usage
- Use sub-agents to search for all `rand()` calls to ensure they're all properly seeded
- Use sub-agents to verify no other code depends on deterministic rand() behavior

## Overcoming Obstacles
- If the root cause is different from hypothesized, investigate further by:
  1. Adding debug logging to trace UUID values
  2. Checking talloc memory contexts for corruption
  3. Looking for any memory reuse issues

## Post-conditions
- `make` compiles successfully
- `make check` passes
- `make lint` passes
- `/fork` works correctly after `/kill`
- UUIDs are properly randomized in production
- Working tree is clean (all changes committed)

## Deviations
Document any deviation from this plan with reasoning.
