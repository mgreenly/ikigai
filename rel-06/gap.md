# Agent Implementation Gaps

This document tracks remaining work for the rel-06 multi-agent implementation.

## Active Bugs

These bugs need investigation and fixes before release:

### Bug 1: `/fork` fails with duplicate key constraint after `/kill`

**Reproduction:**
1. Start ikigai with existing agents (root + child)
2. Kill child agent: `/kill <child-uuid>`
3. Attempt to fork: `/fork`

**Observed:**
```
Error: Failed to insert agent: ERROR:  duplicate key value violates unique constraint "agents_pkey"
DETAIL:  Key (uuid)=(wlT4G-jnTXaaLmMzn8maZg) already exists.
```

**Investigation:**
- The UUID in error is the **root agent's UUID**, not the killed child
- `ik_agent_create()` (src/agent.c:42) calls `ik_generate_uuid(agent)` to create unique UUIDs
- `cmd_fork()` (src/commands_agent.c:489) creates child with `ik_agent_create(repl, repl->shared, parent->uuid, &child)`
- Database insert (src/commands_agent.c:508) uses `child->uuid` which should be unique
- The error suggests `child->uuid` somehow equals parent UUID after creation
- Needs investigation of UUID generation or child agent creation logic

### Bug 2: Top separator missing right half

**Observed:**
```
───────────────────────────────────── ↑- ←- [wlT4G-...] →- ↓-
```

**Expected:**
Should fill full width like bottom separator:
```
──────────────────────────────────────────────────────────────────────────────────────────────────────
```

**Investigation:**
- Separator rendering (src/layer_separator.c:66-193) calculates width as: `sep_chars = width - info_len`
- `info_len = nav_len + debug_len` (line 168)
- Navigation string (lines 80-124) includes ANSI escape codes (e.g., `ANSI_DIM`, `ANSI_RESET`)
- `nav_len` calculated by `snprintf()` (line 122) includes byte count of ANSI codes
- **Root cause**: ANSI escape codes don't consume terminal columns but are counted in string length
- This makes `nav_len` larger than visual width, causing `sep_chars` to be too small
- Result: Total rendered width = `(width - inflated_info_len) + actual_visual_info_len < width`

### Bug 3: Ctrl+Down doesn't switch to child agent

**Reproduction:**
1. Start with root agent that has a child
2. Verify navigation shows `↓-` enabled (correct)
3. Press Ctrl+Down

**Expected:** Switch to child agent

**Observed:** Nothing happens (no switch, no feedback, no error)

**Investigation:**
- Input parsing works: Ctrl+Down generates `IK_INPUT_NAV_CHILD` (tests/unit/input/nav_arrows_test.c:100-127)
- Action routing works: NAV_CHILD calls `ik_repl_nav_child(repl)` (src/repl_actions.c:266-267)
- `ik_repl_nav_child()` (src/repl.c:519-543) searches for children where `a->created_at > newest_time`
- Database shows child has `created_at=0` (legacy data from before created_at was added)
- Comparison fails: `0 > 0` is false, so child is never selected
- **Root cause**: Old agent data with `created_at=0` fails the time-based selection logic
- Navigation context update correctly counts children (update_nav_context scans same array)
- This explains why `↓-` shows enabled (child exists) but switch fails (created_at comparison fails)

### Data Inconsistency: `/agents` command doesn't show dead agents

**Observation:**
After `/kill <uuid>`, the `/agents` command shows "1 running, 0 dead" but the database contains a dead agent.

**Investigation:**
- `/agents` command (src/commands_agent_list.c:51) calls `ik_db_agent_list_running()`
- Query (src/db/agent.c:203) has `WHERE status = 'running'` - only returns running agents
- Dead agents exist in database but are excluded from `/agents` output
- The "0 dead" count is accurate for the displayed list (which has no dead agents)
- **Clarification**: "Dead" means agents in the database with `status='dead'`
- They are not shown in the tree output, only counted in the summary
- Current behavior: Only running agents are displayed; dead count always shows 0

**Question**: Is this intentional behavior or should dead agents be shown in the tree?

---

## Summary

**Core persistence feature is COMPLETE:**
- Gap 0: Message Type Unification ✓
- Gap 1: Startup Agent Restoration Loop ✓
- Gap 5: Session Restore Migration ✓
- Gap 8: Separator Navigation Context Wiring ✓

Multi-agent sessions now persist correctly across restarts with proper parent-child hierarchy restoration.

**Remaining work:** Three low-priority gaps (9, 10, 11) are deferred for future releases.

---

## Future Work (Deferred)

The following gaps are acknowledged but deferred for future releases:

### Gap 9: Lower Separator Per-Agent

**Status:** LOW priority - minor visual issue

The lower separator layer is created once and owned by `repl`, but added to `repl->current->layer_cake`. When switching agents, each has its own layer_cake, so the lower separator isn't moved.

**Impact:** Minor visual inconsistency when switching agents.

**Fix options:**
1. Make lower separator per-agent (create in `ik_agent_create`)
2. Or move layer on switch in `ik_repl_switch_agent()`

### Gap 10: Multi-Agent Tool Polling

**Status:** LOW priority - edge case

The event loop only checks `repl->current` for tool thread completion. If:
1. Agent A starts a tool
2. User switches to Agent B
3. Tool on Agent A completes

The completion isn't detected until user switches back to Agent A.

**Impact:** Low - tools typically complete quickly. Tool result will be processed when user switches back. No data loss, just delayed processing.

**Fix:** Iterate all agents for tool completion in event loop.

### Gap 11: Scrollback Empty at Fork Time (UX)

**Status:** Design clarification - intentional behavior

When forking:
- Conversation: Full parent history copied (for LLM context)
- Scrollback: Starts empty (shows only post-fork content)

This is intentional - scrollback IS reconstructed via replay on restart. But UX concern exists:
- User sees empty child scrollback
- But LLM has full conversation context
- Could be confusing

**UX improvements to consider:**
1. Status bar indication: "Inherited: 15 messages"
2. `/context` command to show inherited message count
3. Scrollback header showing inheritance

### Known Data Loss: Mark Timestamps

Mark timestamps (`ik_mark_t.timestamp`) are lost after restart. The replay context doesn't include timestamps - only position and label are preserved. To fix:
- Add `timestamp` to mark event's `data` JSON in database
- Parse timestamp during replay

This is acceptable for now - marks retain position and label.
