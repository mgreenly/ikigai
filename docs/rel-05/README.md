# rel-05: Architecture Refactor (Phase 0)

Restructured code for multi-agent support. No new features, no user-visible changes. See [agent-process-model.md](../agent-process-model.md) for the full design document.

## Goal

Single agent still works, but code is organized so adding agents is straightforward.

## Architecture

```
┌─────────────────────────────────────────┐
│           shared_ctx (singleton)        │
│  ┌─────────┐ ┌──────────┐ ┌──────────┐  │
│  │ terminal│ │  render  │ │ db_pool  │  │
│  │  input  │ │          │ │          │  │
│  └─────────┘ └──────────┘ └──────────┘  │
└─────────────────────────────────────────┘
                    │
                    │ repl_ctx.current
                    ▼
              ┌──────────────┐
              │  agent_ctx   │
              │  (agent 0)   │
              │ ┌──────────┐ │
              │ │scrollback│ │
              │ │ llm_conn │ │
              │ │ history  │ │
              │ │input_buf │ │
              │ │scroll_pos│ │
              │ │  uuid    │ │
              │ │parent_id │ │
              │ └──────────┘ │
              └──────────────┘
```

**shared_ctx** (one per terminal):
- Terminal I/O (stdin/stdout)
- Render loop
- Input event processing
- Database connection pool
- Pointer to current agent

**agent_ctx** (one per agent):
- Scrollback buffer
- LLM connection/streaming state
- Conversation history
- Saved input buffer (preserved on switch)
- Saved scroll position
- UUID, parent relationship

## Completed Work

- Define `shared_ctx` struct (terminal, render, input)
- Define `agent_ctx` struct (scrollback, llm, history, state)
- Extract current monolithic code into these two structures
- `repl_ctx.current` points to single agent
- All existing functionality preserved

**Completed tasks:**
- Cleanup: removed legacy logger, scroll debug cleanup
- Agent context extraction: struct, display, input, conversation, LLM, tool, completion, spinner
- Agent context integration: single agent init, agent pointer in repl_ctx

## Next Steps

See [rel-06](../rel-06/README.md) for Phases 1-7 (Registry, Switching, Fork, History, Replay, Signals, Mailbox).

## Reference

Previous rel-05 work preserved in `docs/rel-05.bak/` for reference:
- User stories with interaction transcripts
- Task files with TDD structure
- Useful for edge cases and UI patterns
