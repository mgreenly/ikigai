# Task: Migrate tool state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 6 (tool fields migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)
- docs/rel-05/scratch.md (ik_agent_ctx_t tool fields)

## Pre-read Source (patterns)
- src/agent.h (current agent context)
- src/agent.c (current agent create)
- src/repl.h (tool fields to migrate)
- src/repl_init.c (tool state initialization)
- src/tool.h (ik_tool_call_t)
- src/repl.c (tool thread handling)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Display, input, conversation, and LLM fields already migrated
- `repl->agent` exists with LLM state

## Task
Migrate tool execution state from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `pending_tool_call` - tool call awaiting execution
- `tool_thread` - worker thread handle
- `tool_thread_mutex` - protects tool_thread_* fields
- `tool_thread_running` - thread is active
- `tool_thread_complete` - thread finished, result ready
- `tool_thread_ctx` - memory context for thread
- `tool_thread_result` - result JSON from tool dispatch
- `tool_iteration_count` - tool loop iteration tracking

After this task:
- Agent owns its tool execution state
- Each agent can independently execute tools
- Access pattern becomes `repl->agent->pending_tool_call`, etc.

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add forward declaration:
     ```c
     typedef struct ik_tool_call ik_tool_call_t;
     ```
   - Add includes:
     ```c
     #include <pthread.h>
     #include <stdbool.h>
     ```
   - Add fields:
     ```c
     // Tool execution state (per-agent)
     ik_tool_call_t *pending_tool_call;
     pthread_t tool_thread;
     pthread_mutex_t tool_thread_mutex;
     bool tool_thread_running;
     bool tool_thread_complete;
     TALLOC_CTX *tool_thread_ctx;
     char *tool_thread_result;
     int32_t tool_iteration_count;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->pending_tool_call` is NULL initially
   - Test `agent->tool_thread_running` is false initially
   - Test `agent->tool_thread_complete` is false initially
   - Test `agent->tool_iteration_count` is 0 initially
   - Test mutex is initialized (can lock/unlock)

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Initialize tool state:
     ```c
     agent->pending_tool_call = NULL;
     agent->tool_thread_running = false;
     agent->tool_thread_complete = false;
     agent->tool_thread_ctx = NULL;
     agent->tool_thread_result = NULL;
     agent->tool_iteration_count = 0;

     int mutex_result = pthread_mutex_init(&agent->tool_thread_mutex, NULL);
     if (mutex_result != 0) {
         talloc_free(agent);
         return ERR(ctx, IK_ERR_SYSTEM, "Failed to initialize tool thread mutex");
     }
     ```
   - Add talloc destructor to destroy mutex on agent free:
     ```c
     static int agent_destructor(ik_agent_ctx_t *agent)
     {
         pthread_mutex_destroy(&agent->tool_thread_mutex);
         return 0;
     }
     // In ik_agent_create:
     talloc_set_destructor(agent, agent_destructor);
     ```

2. Update `src/repl.h`:
   - Remove all tool fields listed above

3. Update `src/repl_init.c`:
   - Remove tool state initialization (now in agent_create)
   - Remove mutex initialization (now in agent_create)

4. Update `src/repl_cleanup.c` (if exists) or `ik_repl_cleanup`:
   - Remove mutex destruction (now in agent destructor)

5. Update ALL files that access tool state:
   - Change `repl->pending_tool_call` to `repl->agent->pending_tool_call`
   - Change `repl->tool_thread` to `repl->agent->tool_thread`
   - Change `repl->tool_thread_mutex` to `repl->agent->tool_thread_mutex`
   - Change all other tool field accesses similarly

6. Run `make check` - expect pass

### Refactor
1. Verify mutex is properly initialized and destroyed
2. Verify tool thread functions work with agent context
3. Verify tool_thread_ctx ownership is correct
4. Verify no direct tool field access remains in repl_ctx
5. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Tool fields are in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- Mutex properly managed via talloc destructor
- Tool execution functions work with agent context
- All tool state access uses `repl->agent->*` pattern
- 100% test coverage maintained
- Working tree is clean (all changes committed)
