# Task: Migrate conversation state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 4 (conversation fields migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/rel-05/scratch.md (ik_agent_ctx_t conversation fields)

## Pre-read Source (patterns)
- src/agent.h (current agent context)
- src/agent.c (current agent create)
- src/repl.h (conversation fields to migrate, ik_mark_t definition)
- src/repl_init.c (conversation initialization)
- src/openai/client.h (ik_openai_conversation_t)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- `make check` passes
- Display and input fields already migrated to agent context
- `repl->agent` exists with scrollback, input_buffer

## Task
Migrate conversation state fields from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `conversation` - message array for LLM
- `marks` - checkpoint array
- `mark_count` - number of marks

Also move `ik_mark_t` typedef to agent.h (or a separate marks.h if cleaner).

After this task:
- Agent owns its conversation history
- Each agent has independent conversation with LLM
- Access pattern becomes `repl->agent->conversation`, etc.

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add or move `ik_mark_t` typedef:
     ```c
     typedef struct {
         size_t message_index;
         char *label;
         char *timestamp;
     } ik_mark_t;
     ```
   - Add forward declaration:
     ```c
     typedef struct ik_openai_conversation ik_openai_conversation_t;
     ```
   - Add fields:
     ```c
     // Conversation state (per-agent)
     ik_openai_conversation_t *conversation;
     ik_mark_t **marks;
     size_t mark_count;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->conversation` is initialized
   - Test `agent->marks` is NULL initially
   - Test `agent->mark_count` is 0 initially

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Add include for openai/client.h
   - Initialize conversation:
     ```c
     agent->conversation = ik_openai_conversation_create(agent);
     ```
   - Initialize marks:
     ```c
     agent->marks = NULL;
     agent->mark_count = 0;
     ```

2. Update `src/repl.h`:
   - Remove `ik_mark_t` typedef (moved to agent.h)
   - Remove `ik_openai_conversation_t *conversation;` field
   - Remove `ik_mark_t **marks;` field
   - Remove `size_t mark_count;` field

3. Update `src/repl_init.c`:
   - Remove conversation creation (now in agent_create)
   - Remove marks initialization (now in agent_create)

4. Update ALL files that access conversation fields:
   - Change `repl->conversation` to `repl->agent->conversation`
   - Change `repl->marks` to `repl->agent->marks`
   - Change `repl->mark_count` to `repl->agent->mark_count`

5. Update files that reference `ik_mark_t`:
   - Add include for agent.h where needed
   - Or if ik_mark_t was moved to separate header, include that

6. Run `make check` - expect pass

### Refactor
1. Verify conversation is properly owned by agent (talloc child)
2. Verify marks array ownership is correct
3. Verify /mark and /rewind commands work with new structure
4. Verify no direct conversation field access remains in repl_ctx
5. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Conversation fields are in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- `ik_mark_t` typedef accessible from agent.h
- `ik_agent_create()` initializes all conversation state
- All conversation access uses `repl->agent->*` pattern
- 100% test coverage maintained
