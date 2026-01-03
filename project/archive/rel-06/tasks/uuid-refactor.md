# Task: Refactor UUID Generation to Standalone Module

## Target
User Story: 01-agent-registry-persists.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/naming.md (module organization)

## Pre-read Source (patterns)
- src/agent.c (READ - current location of ik_agent_generate_uuid)
- src/agent.h (READ - current declaration)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c (READ - existing UUID tests)

## Files to Create
- src/uuid.h (CREATE - new header for UUID utilities)
- src/uuid.c (CREATE - new implementation file)
- tests/unit/uuid_test.c (CREATE - new test file for UUID module)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- All previous tasks in order.json are complete

## Task
Refactor UUID generation from agent-specific to generic utility. The function `ik_agent_generate_uuid()` currently exists in `src/agent.c` but is a general-purpose utility needed by multiple modules.

**Changes:**
1. Rename `ik_agent_generate_uuid()` to `ik_generate_uuid()`
2. Move to new files: `src/uuid.c` and `src/uuid.h`
3. Update all references (src/agent.c, src/agent.h, tests)

**API:**
```c
// Generate a base64url-encoded UUID (22 characters)
// Caller owns returned string (allocated on ctx)
char *ik_generate_uuid(TALLOC_CTX *ctx);
```

**Rationale (from questions.md Q3):**
- Function is generic, not agent-specific
- Will be used by multiple modules (agents, mail, etc.)
- Follows single-responsibility principle

## TDD Cycle

### Red
1. Create `src/uuid.h`:
   ```c
   #ifndef UUID_H
   #define UUID_H

   #include <talloc.h>

   // Generate a base64url-encoded UUID (22 characters)
   char *ik_generate_uuid(TALLOC_CTX *ctx);

   #endif
   ```

2. Create `tests/unit/uuid_test.c`:
   - Test ik_generate_uuid returns 22 character string
   - Test ik_generate_uuid returns valid base64url characters
   - Test ik_generate_uuid returns unique values
   - Test ik_generate_uuid result owned by ctx

3. Run `make check` - expect test failures

### Green
1. Create `src/uuid.c`:
   - Move implementation from src/agent.c
   - Update function name to ik_generate_uuid

2. Update `src/agent.c`:
   - Remove ik_agent_generate_uuid implementation
   - Include uuid.h
   - Replace calls to ik_agent_generate_uuid with ik_generate_uuid

3. Update `src/agent.h`:
   - Remove ik_agent_generate_uuid declaration

4. Update `tests/unit/agent/agent_test.c`:
   - Replace ik_agent_generate_uuid with ik_generate_uuid
   - Include uuid.h

5. Update Makefile to compile uuid.c

6. Run `make check` - expect pass

### Refactor
1. Verify no remaining references to ik_agent_generate_uuid
2. Run `make lint` - verify clean
3. Commit changes with appropriate message

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)
- For repetitive changes across multiple files (updating function calls), consider using haiku sub-agents to parallelize the work

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_generate_uuid()` exists in src/uuid.c
- No references to ik_agent_generate_uuid remain
- Working tree is clean (all changes committed)
