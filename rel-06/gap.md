# Agent Implementation Gaps

This document tracks remaining work for the rel-06 multi-agent implementation.

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
