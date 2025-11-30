# Platform Architecture

Ikigai is built on a layered architecture: a core C library (`libikigai`) provides the intelligence, and multiple interfaces expose it to developers and agents.

```
                     Developer                        Agents
                         │                              │
                         ▼                              ▼
              ┌─────────────────────┐      ┌─────────────────────┐
              │   Ikigai Terminal   │      │    Ikigai Daemon    │
              │       (TUI)         │      │   (Unix Socket)     │
              └──────────┬──────────┘      └──────────┬──────────┘
                         │                            │
                         └──────────┬─────────────────┘
                                    ▼
              ┌─────────────────────────────────────────────────┐
              │                  libikigai (C)                  │
              │   LLM Routing • Conversations • Tool Execution  │
              │   Platform Tools • Telemetry • Provider APIs    │
              └─────────────────────────────────────────────────┘
                                    │
          ┌─────────────────────────┼─────────────────────────┐
          ▼                         ▼                         ▼
   ┌─────────────┐         ┌───────────────┐         ┌───────────────┐
   │ LLM Providers│        │  PostgreSQL   │         │    Agents     │
   │ (OpenAI,    │         │ (conversations│         │   (Deno)      │
   │  Anthropic) │         │  telemetry)   │         │               │
   └─────────────┘         └───────────────┘         └───────────────┘
```

---

## The Core: libikigai

`libikigai` is a C library containing all the intelligence of the platform:

- **LLM Provider Abstraction**: Unified interface to OpenAI, Anthropic, Google, X.AI
- **Conversation Management**: Multi-turn threads, history, context optimization
- **Tool Framework**: Define tools, execute them, handle results
- **Platform Tools**: Built-in tools for querying telemetry, creating tasks, sending messages
- **Telemetry Emission**: Structured logging of all LLM interactions
- **PostgreSQL Integration**: Persistent conversation storage, full-text search

The library knows nothing about user interfaces. It provides capabilities; interfaces consume them.

---

## Two Interfaces, One Library

### Ikigai Terminal

The [Terminal](05-terminal.md) is the developer's interface:

- TUI with interactive input/output
- Session management and history
- Local file access for code context
- SSH tunneling to remote servers

It links against `libikigai` and adds the interactive layer.

### Ikigai Daemon

The [Daemon](06-runtime.md#ikigai-daemon) is the agent's interface:

- Listens on a Unix socket
- Accepts prompt requests from agents
- Returns structured responses
- Enforces per-agent permissions

It links against the same `libikigai` and exposes it as a service.

---

## The Four Pillars

| Pillar | Component | Purpose |
|--------|-----------|---------|
| 1 | [Ikigai Terminal](05-terminal.md) | Developer's conversational interface to the platform |
| 2 | [Runtime System](06-runtime.md) | Coordination infrastructure (queues, mailboxes, **LLM via daemon**) |
| 3 | [Autonomous Agents](07-agents.md) | Long-running Deno/TypeScript processes |
| 4 | [Web Portal](08-web-portal.md) | Browser-based access to the platform |

Each pillar is documented in detail in its own section.

---

## Why This Architecture

Factoring the intelligence into a library enables:

1. **No code duplication**: LLM routing, tool execution, telemetry exist once
2. **Consistent behavior**: Developers and agents use the same capabilities
3. **Independent evolution**: Improve the TUI without touching daemon code
4. **Testability**: `libikigai` can be tested in isolation
5. **Future interfaces**: Web UI, VS Code extension, mobile app could all consume the library
