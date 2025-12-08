# Pillar 1: Ikigai Terminal

> This is the first of [four pillars](02-architecture.md) that make up the Ikigai platform. The Terminal is your interface to everything else.

---

## Purpose

Ikigai Terminal is the developer's primary interface to the platform. It is a conversational coding agent that understands the runtime environment, deployment model, and operational concerns. Unlike general-purpose coding assistants, Ikigai is purpose-built for this platform.

The Terminal is one of two interfaces to `libikigai`, the core C library. The other is the [Ikigai Daemon](06-runtime.md#ikigai-daemon), which exposes the same capabilities to agents.

---

## The Core: libikigai

The Terminal's intelligence comes from `libikigai`, a C library that handles:

- **LLM Provider Abstraction**: Unified API for OpenAI, Anthropic, Google, X.AI
- **Conversation Management**: Multi-turn threads, context windows, history
- **Tool Framework**: Define tools, execute them, parse results
- **Platform Tools**: Query telemetry, create tasks, send messages, deploy agents
- **Telemetry**: Structured logging of all LLM interactions
- **PostgreSQL Integration**: Persistent conversations, full-text search

The Terminal adds the interactive layer: TUI rendering, keyboard input, session management, and SSH tunneling.

```
┌─────────────────────────────────────────────────────────┐
│                   Ikigai Terminal                        │
│  ┌───────────────────────────────────────────────────┐  │
│  │  TUI Layer: Input • Rendering • Sessions • SSH   │  │
│  └───────────────────────────────────────────────────┘  │
│                          │                               │
│  ┌───────────────────────▼───────────────────────────┐  │
│  │                    libikigai                       │  │
│  │  LLM Routing • Tools • Conversations • Telemetry  │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## Capabilities

### Development

- Generate agent scaffolding from natural language descriptions
- Write TypeScript code following platform conventions
- Understand queue/mailbox patterns and generate correct integration code
- Run and test agents locally using the same `agents.d/` structure as production

### Deployment

- Deploy agents to production with a single command
- Clone git tags directly to servers (no artifact pipeline)
- Create Linux users, PostgreSQL roles, directories automatically on first deploy
- Manage systemd services, perform health checks, handle rollbacks

### Operations

- View agent status, logs, and metrics
- Query telemetry data from PostgreSQL
- Start, stop, restart agents
- Inspect queue contents and mailbox messages
- Set and manage secrets

---

## Developer Experience

The developer's mental model is conversational:

```
> create a monitoring agent that checks API health every 5 minutes
  and alerts to the ops-alerts mailbox if anything is down

[Ikigai scaffolds the agent, explains the structure]

> run it locally

[Ikigai starts the agent, shows output]

> looks good, deploy to production

[Ikigai builds, transfers, deploys, verifies health]

> show me the last hour of alerts

[Ikigai queries the mailbox, displays results]
```

The complexity of Linux users, systemd units, PostgreSQL roles, file permissions, and CI/CD pipelines exists but is invisible to the developer during normal operation.

---

## Technical Implementation

Ikigai Terminal is installed on the developer's local machine, not on production servers.

**Terminal binary** (`ikigai-terminal`):
- Links against `libikigai`
- Direct terminal rendering with UTF-8 support
- Session management and history navigation
- SSH-based remote server operations using the developer's credentials

**Core library** (`libikigai`):
- Written in C for performance and stability
- PostgreSQL-backed conversation history with full-text search
- Multi-provider LLM integration
- Tool execution framework
- Platform tool implementations

The same `libikigai` powers both the Terminal and the [Daemon](06-runtime.md#ikigai-daemon). When you improve the library, both interfaces benefit.

---

**Next**: [Runtime System](06-runtime.md), the coordination infrastructure that agents use
