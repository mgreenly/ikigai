# Task: Agent Context Cleanup and Verification

## Target
Phase 1: Agent Context Extraction - Final Verification

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/backlog/shared-context-di.md (success criteria)
- docs/rel-05/scratch.md (Phase 1 design)

### Pre-read Source (patterns)
- src/agent.h (complete agent context)
- src/agent.c (complete agent create)
- src/shared.h (shared context)
- src/repl.h (repl context with shared + agent pointers)
- src/repl_init.c (initialization flow)
- src/client.c (main entry point)

### Pre-read Tests (patterns)
- tests/unit/agent/*.c
- tests/helpers/test_contexts.h

## Pre-conditions
- `make check` passes
- All Phase 1 field migrations complete
- `repl->agent` holds single agent with all per-agent state

## Task
Final cleanup and verification for Phase 1 completion:
1. Ensure no stale field references remain
2. Verify ownership hierarchy is correct
3. Update test helpers to create agent
4. Add documentation comments
5. Verify all per-agent fields extracted

This is a verification/polish task, not new implementation.

## TDD Cycle

### Red (Verification)
1. Search codebase for any remaining direct field access that should go through agent:
   - `repl->scrollback` (should be `repl->agent->scrollback`)
   - `repl->layer_cake` (should be `repl->agent->layer_cake`)
   - `repl->input_buffer` (should be `repl->agent->input_buffer`)
   - `repl->conversation` (should be `repl->agent->conversation`)
   - `repl->state` (should be `repl->agent->state`)
   - `repl->multi` (should be `repl->agent->multi`)
   - `repl->pending_tool_call` (should be `repl->agent->pending_tool_call`)
   - All other per-agent fields

2. Verify `ik_repl_ctx_t` now only contains:
   - `ik_shared_ctx_t *shared` - shared infrastructure
   - `ik_agent_ctx_t *agent` - single agent (for now)
   - `ik_input_parser_t *input_parser` - stateless parser
   - `atomic_bool quit` - exit flag

3. Verify ownership hierarchy:
   ```
   root_ctx
     ├─> shared_ctx (sibling)
     └─> repl_ctx (sibling)
              └─> agent (child of repl)
   ```

4. Run `make check` - should already pass

### Green (Cleanup)
1. Fix any remaining stale references found in verification

2. Update `tests/helpers/test_contexts.c`:
   - `test_repl_create()` should create agent as part of repl setup
   - Verify test helpers work with new structure

3. Add documentation to `src/agent.h`:
   ```c
   /**
    * Per-agent context for ikigai.
    *
    * Contains all state specific to one agent:
    * - Identity (agent_id, numeric_id)
    * - Display state (scrollback, layers)
    * - Input state (input_buffer, visibility)
    * - Conversation (messages, marks)
    * - LLM interaction (curl_multi, streaming buffers)
    * - Tool execution (thread, mutex, pending calls)
    * - UI state (spinner, completion)
    *
    * Ownership: Created as child of ik_repl_ctx_t.
    * Currently single agent per REPL. Multi-agent support
    * will add agents[] array (manual-top-level-agents work).
    *
    * Thread safety: Tool execution uses mutex for thread safety.
    * Agent fields should only be accessed from main thread
    * except where explicitly synchronized.
    */
   ```

4. Add documentation to `ik_repl_ctx_t` in `src/repl.h`:
   ```c
   // Per-agent state (currently single agent, will become array)
   // All per-agent fields accessed via this pointer
   ik_agent_ctx_t *agent;
   ```

5. Verify initialization flow in client.c/repl_init.c:
   ```c
   // 1. Load config
   // 2. Create shared context (infrastructure)
   // 3. Create REPL (receives shared)
   //    3a. Create agent (receives shared for dimensions)
   // 4. Run
   // 5. Cleanup (reverse order via talloc)
   ```

6. Run `make check` - expect pass

### Refactor
1. Run full quality checks:
   - `make lint` - no violations
   - `make coverage` - 100% coverage
   - Valgrind/sanitizers - no memory errors

2. Verify Phase 1 architecture:
   - [ ] `ik_agent_ctx_t` contains all per-agent state
   - [ ] `ik_repl_ctx_t` contains only coordinator state + agent pointer
   - [ ] Agent created via `ik_agent_create(repl, shared, 0, &agent)`
   - [ ] Clear ownership hierarchy
   - [ ] Access pattern: `repl->agent->field` for per-agent, `repl->shared->field` for shared

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100%
- No stale field references in codebase
- Documentation comments added
- Test helpers updated
- **Phase 1 complete - ready for multi-agent work**
