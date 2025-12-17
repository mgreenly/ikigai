# Task: Final Verification for Gap 1 + Gap 5

## Target
Gap 1 + Gap 5 merged: Startup Agent Restoration Loop (PR2) - Final Verification

## Macro Context

This is the final verification step for Gap 1 and Gap 5 (merged). We confirm that the entire agent restoration system is complete and working correctly.

**What was implemented across prior Gap 1 tasks:**

1. **gap1-agent-restore-fn.md** - Created `ik_agent_restore()` function that creates an agent context from a DB row (uses DB data instead of generating new UUID/timestamps)

2. **gap1-restore-agents-skeleton.md** - Created `ik_repl_restore_agents()` skeleton in `src/repl/agent_restore.c` with proper structure for restoring all running agents

3. **gap1-restore-loop-body.md** - Implemented the restoration loop body:
   - Queries all running agents from DB
   - Sorts by created_at (oldest first - parents before children)
   - For each agent: restores context, replays history, populates conversation/scrollback

4. **gap1-integrate-repl-init.md** - Integrated `ik_repl_restore_agents()` into `ik_repl_init()` startup sequence

5. **gap1-migrate-agent0.md** - Migrated Agent 0 to use the same restoration path as other agents (eliminates special-case code)

6. **gap1-fresh-install.md** - Handled fresh install case (no agents in DB - creates Agent 0 with clear event and system message)

7. **gap1-delete-obsolete.md** - Deleted obsolete session restore code:
   - Removed `src/repl/session_restore.c`
   - Removed `src/repl/session_restore.h`
   - Removed `ik_db_messages_load()` (no longer needed)

8. **gap1-tests.md** - Created comprehensive tests for the restoration system

**What this task verifies:**

- All components work together correctly
- No regressions from prior functionality
- Code quality (tests pass, lint clean, 100% coverage)
- No orphaned references to deleted code
- Architectural correctness (Agent 0 uses same path as other agents)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/source-code.md

## Pre-read Docs
- rel-06/gap.md (Gap 1 and Gap 5 sections, especially "Impact" and "Testing Considerations")

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- All prior Gap 1 tasks completed:
  - gap1-agent-restore-fn.md
  - gap1-restore-agents-skeleton.md
  - gap1-restore-loop-body.md
  - gap1-integrate-repl-init.md
  - gap1-migrate-agent0.md
  - gap1-fresh-install.md
  - gap1-delete-obsolete.md
  - gap1-tests.md

## Task

### Part 1: Code Quality Checks

Run all quality checks:

```bash
make clean
make check          # All tests pass
make lint           # No lint errors
make coverage       # 100% coverage maintained
```

If any check fails, investigate and fix before proceeding.

### Part 2: Grep Verification - Ensure Cleanup Complete

Verify no references to deleted code remain:

```bash
# No references to deleted session restore function
grep -r "ik_repl_restore_session" src/    # Should find nothing

# No references to deleted message load function (unless intentionally kept)
grep -r "ik_db_messages_load" src/        # Should find nothing

# No references to deleted session restore headers
grep -r "session_restore.h" src/          # Should find nothing

# No references to old message type (Gap 0 cleanup)
grep -r "ik_message_t" src/               # Should find nothing
```

If any results found in source files, they are bugs from prior tasks and must be fixed.

### Part 3: Architectural Verification

Verify the architectural requirements are met:

1. **`ik_agent_restore()` exists and is used:**
   ```bash
   grep -r "ik_agent_restore" src/
   ```
   Should find declaration in `agent.h`, implementation in `agent.c`, and usage in `agent_restore.c`.

2. **`ik_repl_restore_agents()` exists and is called from `ik_repl_init()`:**
   ```bash
   grep -r "ik_repl_restore_agents" src/
   ```
   Should find declaration in `agent_restore.h`, implementation in `agent_restore.c`, and call in `repl_init.c`.

3. **`session_restore.c` and `session_restore.h` are deleted:**
   ```bash
   ls src/repl/session_restore.c 2>/dev/null && echo "ERROR: file exists" || echo "OK: file deleted"
   ls src/repl/session_restore.h 2>/dev/null && echo "ERROR: file exists" || echo "OK: file deleted"
   ```
   Both should report "OK: file deleted".

4. **Agent 0 goes through same restoration path as other agents:**
   Review `ik_repl_restore_agents()` to confirm Agent 0 is not special-cased (all agents use same restore logic).

5. **Agent 0 is current after restore (Gap 6 verification):**
   Verify that after `ik_repl_restore_agents()` completes, `repl->current` is still Agent 0.
   This ensures consistent startup behavior - user always starts at the root agent.
   ```bash
   # In test or manual verification:
   # After restore, repl->current->parent_uuid should be NULL (Agent 0 is root)
   ```

### Part 4: Functional Verification

Document what manual testing would verify (for QA reference):

1. **Fresh install works:**
   - Delete database (or use fresh session)
   - Start ikigai
   - Verify Agent 0 is created with clear event and system message
   - Verify scrollback shows system welcome message

2. **Single agent restart works (Agent 0 history preserved):**
   - Start ikigai, interact with Agent 0, create some history
   - Exit and restart
   - Verify Agent 0's conversation history is restored
   - Verify scrollback shows previous interactions

3. **Multi-agent restart works (forked agents restored with correct hierarchy):**
   - Start ikigai, fork an agent from Agent 0
   - Interact with both agents
   - Exit and restart
   - Verify both agents exist
   - Verify parent-child hierarchy preserved
   - Verify each agent's conversation is correct (fork point respected)

4. **Killed agents not restored:**
   - Start ikigai, fork an agent, kill the forked agent
   - Exit and restart
   - Verify killed agent is NOT restored (only running agents restored)

### Part 5: Documentation Update

Update `rel-06/gap.md` to mark Gap 1 and Gap 5 as COMPLETE:

1. In Gap 1 section, add status line: `**Status:** COMPLETE`
2. In Gap 5 section, add status line: `**Status:** COMPLETE (merged into Gap 1)`
3. In the Summary section, update to reflect completion
4. In Priority Order section, mark Gap 1 and Gap 5 as done

### Part 6: Report Issues Found

If any issues are found during verification:

1. Document the issue clearly (what was expected vs. what was found)
2. If it's a minor fix, create a fix commit
3. If it's a significant issue, document it in your final report and create a follow-up task if needed

## Sub-agent Usage
- Use sub-agents to run verification commands in parallel
- Use sub-agents for grep searches

## Post-conditions
- `make check` passes (all tests green)
- `make lint` passes
- `make coverage` shows 100% coverage (or documents exceptions)
- No grep results for deleted code references in src/
- `ik_agent_restore()` exists and is used
- `ik_repl_restore_agents()` exists and is called from init
- `session_restore.c` and `session_restore.h` are deleted
- Agent 0 uses same restoration path as other agents
- `rel-06/gap.md` updated with Gap 1 and Gap 5 marked COMPLETE
- Working tree is clean (all changes committed)

## Success Criteria

Gap 1 + Gap 5 is complete when:
- Multi-agent persistence works across restarts
- All running agents restored with correct hierarchy
- Fork points respected (children don't see parent's post-fork messages)
- Clear events respected (don't walk past clear)
- Fresh install creates Agent 0 correctly
- All code quality checks pass
- No orphaned references to deleted code
- Documentation updated

## Deviations

If you need to deviate from this plan (e.g., issues found that require significant fixes), document the deviation and reasoning in your final report.
