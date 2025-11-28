# Opus Design Documents

Design documents for the next phase of ikigai. These describe the user experience, not implementation details.

## Documents

| Document | Description |
|----------|-------------|
| [agents.md](agents.md) | Agent types (main, helper, sub-agent), switching, lifecycle |
| [agent-queues.md](agent-queues.md) | Queue and inbox primitives for orchestration |
| [architecture.md](architecture.md) | Database schema and threading model |
| [prompt-processing.md](prompt-processing.md) | Slash command pre-processing, personas |
| [memory-documents.md](memory-documents.md) | Database-backed files via `.memory/` path |
| [git-integration.md](git-integration.md) | Worktrees, branch isolation for helper agents |
| [keybindings.md](keybindings.md) | Keyboard shortcuts |

## Not Yet Documented

- RAG - search across conversation history
- Multi-client - multiple terminals to same database

## Key Design Decisions

1. Two primitives: Queue (dispatch work) + Inbox (receive messages)
2. User acts through main agent, not as separate entity
3. PostgreSQL is the coordination layer
4. Automatic result delivery from sub-agents
5. Prompt pre-processing for `/read` and `/push`
6. Three agent types: main, helper, sub-agent

## Superseded Documentation

- `docs/vision/inter-agent-*.md` - replaced by inbox model
- `docs/task-system.md` - replaced by queue/inbox
- `docs/opus/sub-agents.md` - deleted, replaced by agent-queues.md
