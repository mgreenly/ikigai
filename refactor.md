# Task File Refactoring Plan

Fixes identified during architectural review of `tasks/` directory. All 21 tasks across 4 groups were reviewed by sub-agents.

## Overview

| Group | Tasks | Critical Issues | Model Changes |
|-------|-------|-----------------|---------------|
| Agent Context Decomposition | 5 | 2 | 4 |
| Agent Migration | 4 | 2 | 3 |
| Init Pattern Standardization | 8 | 1 | 7 |
| Namespace Pollution Fix | 4 | 2 | 2 |

## Execution Order

Tasks cannot run in parallel due to file collisions. Required batching:

```
BATCH 1: Init Tasks (7 tasks, defer mark-decouple)
BATCH 2: Agent Decomposition (5 tasks, sequential)
BATCH 3: Agent Migration (4 tasks, sequential)
BATCH 4: Namespace (4 tasks, add to order.json)
BATCH 5: init-mark-decouple (deferred, reconsider post-refactor)
```

---

## Phase 1: Critical Fixes

### 1.1 Fix agent-compose-subcontexts.md Design Flaw

**File:** `tasks/agent-compose-subcontexts.md`

**Problem:** Lines 106-107 instruct calling factories with `&agent->identity` for embedded structs. This is architecturally impossible - factories that allocate memory cannot work with embedded structs.

**Fix Options:**
- **Option A (Recommended):** Change sub-context factories to initialize in-place rather than allocate:
  ```c
  // Change from:
  res_t ik_agent_identity_create(TALLOC_CTX *ctx, ..., ik_agent_identity_t **out);

  // To:
  res_t ik_agent_identity_init(ik_agent_identity_t *identity, ...);
  ```

- **Option B:** Keep sub-contexts as pointers instead of embedded:
  ```c
  typedef struct ik_agent_ctx {
      ik_agent_identity_t *identity;  // Pointer, not embedded
      // ...
  } ik_agent_ctx_t;
  ```

**Decision:** Use Option A. All sub-context types use `*_init()` for in-place initialization. This aligns with naming conventions (`*_create()` allocates, `*_init()` initializes pre-allocated) and keeps the embedded struct design which is more cache-friendly.

**Also fix:** Remove lines 179-184 which falsely claim production code will compile after struct changes.

**Affected tasks:** All 4 agent struct creation tasks need signature updates if Option A chosen.

### 1.2 Add Missing Grep Pattern to Migration Tasks

**Files:**
- `tasks/agent-migrate-identity-callers.md`
- `tasks/agent-migrate-display-callers.md`
- `tasks/agent-migrate-llm-callers.md`
- `tasks/agent-migrate-tool-callers.md`

**Problem:** Tasks only show grep for `agent->FIELD` pattern but production code extensively uses `repl->current->FIELD` (380 occurrences across 24 files).

**Fix:** Add to each task's "How" section:

```markdown
### Discovery Commands

Search BOTH access patterns:
```bash
# Direct agent access
grep -rn "agent->(uuid|name|parent_uuid|...)" src/

# Indirect via repl->current
grep -rn "repl->current->(uuid|name|parent_uuid|...)" src/
```

The `repl->current->` pattern accounts for 80%+ of callsites.
```

### 1.3 Add Namespace Tasks to order.json

**File:** `tasks/order.json`

**Problem:** Namespace tasks exist but aren't in order.json.

**Fix:** Add after agent migration tasks:

```json
{"task": "namespace-repl-event-handlers.md", "group": "Namespace Pollution Fix", "model": "sonnet", "thinking": "extended"},
{"task": "namespace-commands.md", "group": "Namespace Pollution Fix", "model": "sonnet", "thinking": "extended"},
{"task": "namespace-misc.md", "group": "Namespace Pollution Fix", "model": "sonnet", "thinking": "thinking"},
{"task": "namespace-abbreviations.md", "group": "Namespace Pollution Fix", "model": "sonnet", "thinking": "thinking"}
```

---

## Phase 2: Pre-read List Fixes

### 2.1 Agent Decomposition Tasks

**Add to all 5 tasks:**
```markdown
- tests/unit/agent/meson.build (test registration pattern)
```

**Add to agent-display-struct.md:**
```markdown
- src/layer_spinner.c
- src/layer_completion.c
- src/layer_input.c
- src/layer_separator.c
- src/layer_scrollback.c
```

### 2.2 Agent Migration Tasks

**agent-migrate-identity-callers.md - Add:**
```markdown
- src/commands.c
- src/commands_kill.c
- src/commands_mark.c
- src/repl_actions_llm.c
- src/repl_init.c
- src/repl.c
```

**agent-migrate-display-callers.md - Add:**
```markdown
- src/repl_actions.c
- src/repl_viewport.c
- src/repl_actions_viewport.c
- src/repl_actions_history.c
- src/marks.c
- src/completion.c
- src/commands.c
- src/commands_mail.c
- src/commands_kill.c
- src/commands_mark.c
- src/commands_agent_list.c
```

**agent-migrate-llm-callers.md - Add:**
```markdown
- src/completion.c
- src/repl_viewport.c
- src/marks.c
- src/commands.c
```

**agent-migrate-tool-callers.md - Add:**
```markdown
- src/repl.h
- src/repl_viewport.c
```

### 2.3 Init Pattern Tasks

**Add to init-scrollback-naming.md, init-terminal-naming.md, init-render-naming.md, init-db-mem-ctx-naming.md:**
```markdown
- .agents/skills/naming.md
```

**Remove from init-openai-conv-return.md, init-openai-msg-return.md:**
```markdown
- .agents/skills/naming.md  # Not needed for return type changes
```

### 2.4 Namespace Tasks

**namespace-repl-event-handlers.md - Add:**
```markdown
- src/repl.c (7 callsites)
```

**namespace-commands.md - Add:**
```markdown
- src/commands_fork.c
- src/commands_kill.c
- src/commands_mail.c
- src/commands_agent_list.c
```

**namespace-misc.md - Add:**
```markdown
- src/commands_kill.c
- src/repl/agent_restore.c
```

**namespace-abbreviations.md - Add:**
```markdown
- src/shared.c
```

---

## Phase 3: Model/Thinking Level Updates

### 3.1 Update order.json

**Agent Decomposition Group:**
```json
{"task": "agent-identity-struct.md", "model": "sonnet", "thinking": "none"},
{"task": "agent-display-struct.md", "model": "sonnet", "thinking": "thinking"},
{"task": "agent-llm-struct.md", "model": "sonnet", "thinking": "thinking"},
{"task": "agent-tool-executor-struct.md", "model": "sonnet", "thinking": "extended"},
{"task": "agent-compose-subcontexts.md", "model": "sonnet", "thinking": "extended"}
```

**Agent Migration Group:**
```json
{"task": "agent-migrate-identity-callers.md", "model": "sonnet", "thinking": "none"},
{"task": "agent-migrate-display-callers.md", "model": "sonnet", "thinking": "extended"},
{"task": "agent-migrate-llm-callers.md", "model": "sonnet", "thinking": "extended"},
{"task": "agent-migrate-tool-callers.md", "model": "sonnet", "thinking": "extended"}
```

**Init Pattern Group:**
```json
{"task": "init-openai-conv-return.md", "model": "sonnet", "thinking": "none"},
{"task": "init-openai-msg-return.md", "model": "sonnet", "thinking": "none"},
{"task": "init-scrollback-naming.md", "model": "sonnet", "thinking": "none"},
{"task": "init-terminal-naming.md", "model": "sonnet", "thinking": "none"},
{"task": "init-render-naming.md", "model": "sonnet", "thinking": "none"},
{"task": "init-db-mem-ctx-naming.md", "model": "sonnet", "thinking": "none"},
{"task": "init-mark-decouple.md", "model": "sonnet", "thinking": "extended"},
{"task": "init-db-session-param-order.md", "model": "sonnet", "thinking": "none"}
```

---

## Phase 4: Sub-Agent Guidance

### 4.1 Add Sub-Agent Strategy Section

**Add to tasks with >20 changes:**
- agent-migrate-display-callers.md (51 callsites)
- agent-migrate-llm-callers.md (118 callsites)
- agent-migrate-tool-callers.md (82 callsites)
- namespace-repl-event-handlers.md (212 changes)
- namespace-commands.md (339 changes)

**Template to add:**

```markdown
## Sub-Agent Execution Strategy

This task has X changes across Y files. Use sub-agents to parallelize:

### For Migration Tasks:
1. Spawn one sub-agent per file group (e.g., repl files, command files, db files)
2. Each sub-agent:
   - Grep for all patterns in assigned files
   - Update field access to new pattern (e.g., `agent->uuid` → `agent->identity.uuid`)
   - Verify with `make check`
   - Report completion

### For Namespace Tasks:
1. Group symbols by header file (parallel sub-agents on same file = merge conflicts)
2. Process one header file at a time (sequentially)
3. For each header file group:
   - Update all symbol declarations in the header
   - Update all source definitions
   - Update all callsites across the codebase
   - Report file list

4. After all groups complete:
   - Run `make check` once (not per sub-agent)
   - Verify old names gone with grep post-condition
```

---

## Phase 5: Deferred Work

### 5.1 init-mark-decouple.md

**Status:** Defer to post-agent-refactor

**Reason:**
- Touches `src/agent.h` which conflicts with all agent decomposition tasks
- Task itself acknowledges high complexity and suggests "document as tech debt" option
- After agent decomposition, the new architecture may make decoupling easier or unnecessary

**Action:**
- Remove from current order.json
- Add to backlog/future.json
- Reconsider after Batch 3 completes

---

## Commit Cadence

Apply fixes with **one commit per phase**:

1. `chore(tasks): fix critical design issues` - Phase 1 changes
2. `chore(tasks): complete pre-read lists` - Phase 2 changes
3. `chore(tasks): update model/thinking levels` - Phase 3 changes
4. `chore(tasks): add sub-agent execution guidance` - Phase 4 changes
5. `chore(tasks): defer init-mark-decouple to backlog` - Phase 5 changes

Create `backlog/future.json` if it doesn't exist when applying Phase 5.

---

## Verification Checklist

After applying all fixes, verify:

- [ ] `agent-compose-subcontexts.md` has valid factory approach
- [ ] All migration tasks include `repl->current->` grep pattern
- [ ] All 4 namespace tasks are in order.json
- [ ] All pre-read lists are complete per Phase 2
- [ ] All model/thinking levels updated per Phase 3
- [ ] Sub-agent guidance added to 5 large tasks
- [ ] init-mark-decouple.md removed from todo list

---

## Summary of Changes

| File | Changes |
|------|---------|
| `tasks/order.json` | Add namespace tasks, update thinking levels |
| `tasks/agent-compose-subcontexts.md` | Fix factory design, remove false claim |
| `tasks/agent-migrate-*.md` (4 files) | Add grep pattern, add pre-reads, add sub-agent guidance |
| `tasks/agent-*-struct.md` (4 files) | Add meson.build pre-read |
| `tasks/init-*.md` (8 files) | Fix naming skill pre-reads |
| `tasks/namespace-*.md` (4 files) | Add missing pre-reads, add sub-agent guidance |

**Total files to modify:** 22 task files + order.json
