# Architecture

## Overview

Ikigai's architecture centers on a shared database that serves as the system of record. Everything connects through this single source of truth.

```
                      ┌─────────────────┐
                      │   PostgreSQL    │
                      │  (the system)   │
                      └────────┬────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
  ┌─────┴─────┐          ┌─────┴─────┐          ┌─────┴─────┐
  │  ikigai   │          │autonomous │          │   web     │
  │ terminal/ │          │  agents   │          │ services  │
  │  daemon   │          │  (code)   │          │ (users)   │
  │  (human)  │          │           │          │           │
  └───────────┘          └───────────┘          └───────────┘
        │                      │                      │
        └──────────────────────┴──────────────────────┘
                    same database, same rules
```

**Three ways into one system:**
- **Terminal/daemon**: human decision-maker
- **Autonomous agents**: code decision-maker
- **Web services**: user-facing interfaces that feed/read the agent network

---

## Core Principle: The Database is the System

The PostgreSQL database isn't just storage. It's the coordination layer:

- **Messages**: Agents communicate through database-backed queues
- **History**: Every conversation, decision, and action is recorded
- **Registry**: Agents register themselves, advertise capabilities
- **State**: Shared state lives in the database, not in process memory

This design has important consequences:

1. **No distributed coordination**: Everything goes through one database
2. **Natural durability**: PostgreSQL handles persistence and recovery
3. **Simple debugging**: Query the database to see exactly what happened
4. **Process independence**: Agents can crash and restart, picking up where they left off

---

## The Three Entry Points

### Terminal/Daemon

The ikigai terminal and daemon are two sides of the same coin. The terminal is the interactive interface where a human makes decisions. The daemon is the same code running as a service, where agents connect for LLM access and platform operations.

Both share:
- Identity in the system
- Access to send/receive messages
- History tracking
- Tool capabilities

The difference is who's at the keyboard: human or code.

#### The Shared Intelligence Layer

Both terminal and daemon are built on `libikigai`, a core C library that contains the platform's intelligence:

```
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
```

The library handles:
- **LLM Provider Abstraction**: Unified API for multiple providers
- **Conversation Management**: Multi-turn threads, context windows, history
- **Tool Framework**: Define tools, execute them, parse results
- **Platform Tools**: Query telemetry, create tasks, send messages
- **Telemetry**: Structured logging of all LLM interactions

Agents access LLM capabilities through the daemon via `@ikigai/platform`. This means:
- Unified observability (all LLM calls in one place)
- Centralized API keys
- LLMs can operate the platform (create tasks, send messages)
- Cost tracking per agent

### Autonomous Agents

Agents are TypeScript processes that run continuously. They:
- Register themselves in the database on startup
- Listen for messages in their inbox
- Process messages using LLM reasoning
- Take actions through tools
- Record everything to history

An agent is just a process that talks to the database. No special runtime, no container, no orchestrator. A process with database access.

### Web Services

Web services are the user-facing layer. They don't contain agent logic; they serve as interfaces:
- Accept requests from users
- Push work into agent inboxes
- Read results from the database
- Present information to users

Web services treat agents as async backends. They push, poll, or subscribe. They don't wait for agents to respond in HTTP timeframes.

---

## The Agent Model

Every agent in the system shares the same basic structure:

| Component | Purpose |
|-----------|---------|
| **Identity** | UUID, name, description (business card) |
| **Inbox** | Queue where messages arrive |
| **History** | Record of all messages sent/received |
| **Tools** | Capabilities the agent can use |
| **System prompt** | Internal instructions (how it thinks) |
| **Business card** | External description (what it does) |

The terminal is also an agent. It has an identity, inbox, history, and tools. The only difference is that a human provides the reasoning instead of an LLM.

---

## Communication

Agents communicate through messages:

```
Agent A                              Agent B
    │                                    │
    │  send(to: B, content, convId)      │
    │───────────────────────────────────►│
    │                                    │
    │         (B's inbox queue)          │
    │                                    │
    │◄───────────────────────────────────│
    │       response (same convId)       │
```

Key properties:
- **Point-to-point**: Messages go to specific inboxes
- **Conversation IDs**: Track multi-message exchanges
- **Complete messages**: No streaming, messages arrive whole
- **Dual history**: Both sender and receiver record the exchange

Agents can also subscribe to topics for broadcast-style communication.

---

## Hierarchy and Delegation

Agents form hierarchies through spawning:

```
Human (top of hierarchy, can walk away)
  │
  └── Agent A (long-running, spawns helpers)
        │
        ├── Agent B (specialist)
        │     └── Agent C (sub-task)
        │
        └── Agent D (another specialist)
```

Any agent can spawn other agents. The registry tracks both instance and type, enabling patterns like "spawn another code-reviewer."

This is delegation, not just automation. The human sets direction, top-level agents coordinate, specialists execute.

---

## Tools as Capabilities

Tools define what an agent can do. The human designer curates each agent's toolset:

- File read but not write → can observe but not modify
- Eavesdrop tool → can see other agents' histories
- Spawn tool → can create new agents
- External API tools → can interact with the world

**Tools are the authorization model.** No complex permission system. Just curated toolsets per agent.

---

## Design Philosophy

### Use Linux, Don't Abstract It

Identity is Linux users. Secrets are files with permissions. Processes are managed by runit/systemd. Linux solved these problems decades ago. Ikigai leverages that work.

### Opinionated Defaults

PostgreSQL handles queues, messaging, storage, caching, and telemetry out of the box. One dependency, zero configuration. Swap to specialized backends when specific needs arise.

### Vertical First

Start with one server. A modern Linux machine with fast storage and enough RAM handles hundreds of concurrent agents. No network partitions, no distributed consensus, no eventual consistency. Just processes talking through local sockets and a shared database.

Most teams never need more than this.
