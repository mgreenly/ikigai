# Task: Agent Lookup by UUID

## Target
User Stories: 09-kill-specific.md, 15-send-mail.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Agent Identity section)

## Pre-read Source (READ)
- src/repl.h (ik_repl_ctx_t, ik_repl_find_agent)
- src/agent.h (ik_agent_ctx_t)

## Pre-read Tests (READ)
- tests/unit/repl/ (existing test patterns for reference)

## Files to MODIFY
- src/repl.h (update ik_repl_find_agent signature, add ik_repl_uuid_ambiguous)
- src/repl.c (implement prefix matching logic)

## Files to CREATE
- tests/unit/repl/repl_agent_lookup_test.c (new test file for UUID lookup)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- repl-agent-array.md complete (ik_repl_find_agent exists)

## Task
Enhance agent lookup to support partial UUID matching. Users shouldn't need to type full 22-character UUIDs.

**Enhanced API:**
```c
// Find agent by UUID (exact or prefix match)
// Returns first matching agent, or NULL if no match or ambiguous
ik_agent_ctx_t *ik_repl_find_agent(ik_repl_ctx_t *repl, const char *uuid_prefix);

// Check if UUID prefix is ambiguous (matches multiple agents)
bool ik_repl_uuid_ambiguous(ik_repl_ctx_t *repl, const char *uuid_prefix);
```

**Matching rules:**
- Exact match takes priority
- Prefix match if unambiguous (only one agent matches)
- Return NULL if ambiguous (multiple matches)
- Minimum prefix length: 4 characters

## TDD Cycle

### Red
1. Update function signature in `src/repl.h`

2. Add tests:
   - Test exact match returns correct agent
   - Test prefix match (6 chars) returns correct agent
   - Test ambiguous prefix returns NULL
   - Test uuid_ambiguous returns true for ambiguous prefix
   - Test minimum 4 char prefix enforced

3. Run `make check` - expect test failures

### Green
1. Implement prefix matching in `src/repl.c`
2. Run `make check` - expect pass

### Refactor
1. Consider hash map for O(1) exact lookup if performance needed
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Partial UUID matching works
- Ambiguous prefixes detected
- Working tree is clean (all changes committed)
