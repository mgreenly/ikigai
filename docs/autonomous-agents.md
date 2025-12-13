# Autonomous Agents

## Vision

Autonomous agents are standalone TypeScript processes that run alongside the ikigai terminal. They share the same database, the same messaging infrastructure, and the same identity model. The only difference between an autonomous agent and the ikigai terminal is who makes decisions: code or human.

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

## Core Concept

An autonomous agent is:
- A TypeScript process managed by runit
- Single-threaded: one message at a time, ordered history
- Self-registering: announces itself to the system on startup
- Tool-equipped: capabilities defined by its toolset

The simplest agent runs a blocking loop:

```typescript
import { Agent } from 'ikigai/agent';

const agent = new Agent({
  name: 'code-reviewer',
  businessCard: 'I review code for quality and security issues.',
  systemPrompt: '...',
  tools: [reviewTool, commentTool],
  model: 'claude-sonnet'
});

agent.run();
```

Inside `agent.run()`:
1. Register in the system (identity, tools, business card)
2. Start heartbeat
3. Replay any unacked messages from before crash
4. Enter main loop: wait for message → process with LLM → respond
5. Ack message after successful processing

---

## Communication Model

### Inboxes and Topics

Every agent has:
- **Default inbox** (mandatory): `inbox:{agent_id}` - always listening, cannot be disabled
- **Topic subscriptions** (optional): agent can subscribe to topics, receive copies of published messages

When a message arrives, the agent knows which subscription delivered it (default inbox vs a topic).

### Message Flow

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

### Message Delivery

- **Synchronous send failure**: If the target inbox is full or agent is dead, `send()` fails immediately. The sender knows right away.
- **Complete messages**: Messages are buffered complete, not streamed token-by-token. Simpler protocol, cleaner semantics.
- **Fan-out for topics**: When publishing to a topic, each subscriber gets a copy in their inbox.

### History

Both sender and receiver write to their own history:
- Sender writes: "I sent X to B"
- Receiver writes: "I received X from A"

Duplication in the database, but each agent has a concrete, queryable history. History is the permanent record. The inbox queue is pending/uncertain work.

---

## Agent Identity

### Registry

Agents self-register on startup. The registry contains:

| Field | Description |
|-------|-------------|
| id | UUID (mechanical identifier) |
| name | Human-readable name (from deployment path/config) |
| type | Agent type (enables spawning new instances of same type) |
| description | Business card - external description for other agents |
| tools | Tool schemas (what the agent can do) |
| model | LLM model the agent uses |
| status | running, stopped, crashed |
| last_heartbeat | Health check timestamp |

### Business Card vs System Prompt

- **Business card**: External, visible to other agents. "This is what I do."
- **System prompt**: Internal, the agent's instructions. "This is how I think."

Separate concerns. The human designer ensures consistency.

### Tool Advertisement

Agents advertise their tools in the registry. Other agents can query "who can do X?" and discover capabilities.

---

## Relationship to ikigai Terminal

The ikigai terminal is just another agent with a human at the keyboard.

| Aspect | Terminal | Autonomous Agent |
|--------|----------|------------------|
| Decision maker | Human | Code |
| Identity | Has one | Has one |
| Inbox | Has one | Has one |
| History | Writes it | Writes it |
| Tools | Has them | Has them |
| /commands | Available | Equivalent tool for each |

**Tool/command parity**: Everything an agent can do via tools, humans can do via /slash-commands. Ideally no reserved human-only actions.

The terminal has **privileged tools** for:
- Eavesdropping on any agent's history
- Sending messages to any agent's inbox
- Querying the registry
- Database internals

These are just tools. An autonomous agent could have them too if the designer chooses.

---

## Hierarchy

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

- Any agent can spawn other agents (permissions are loose while we experiment)
- The registry knows both instance and type, enabling "spawn another code-reviewer"
- Initiator/responder relationships form a call stack of conversations

This is **delegation**, not just automation.

---

## Lifecycle Patterns

```
Task Agent          Monitor Agent         Scheduled Agent
    │                    │                      │
 spawned              spawned                spawned
    │                    │                      │
 do work              loop {                 loop {
    │                   wait for event         sleep(interval)
 terminate              handle                 wake
                       }                       do work
                                              }
```

All three are just variations in TypeScript code. The library supports:
- Blocking wait for next message
- Sleep for duration
- Both patterns combined

Agents can run indefinitely (days, weeks) or complete a task and terminate.

---

## Tools as Capabilities

Tools define what an agent can do. The human designer curates each agent's toolset:

- File read but not write → can observe but not modify
- Eavesdrop tool → can see other agents' histories
- Spawn tool → can create new agents
- External API tools → can interact with the world

**Tools are the authorization model** (for now). No complex permission system - just curated toolsets per agent.

---

## Deployment Modes

Autonomous agents run in [iki-genba](iki-genba.md), the PaaS for external agents. The same runtime works locally or on a server.

| Mode | Always-on provided by | Human access |
|------|----------------------|--------------|
| Development | ikigai terminal (C binary) | Direct, at keyboard |
| Server | ikigai-daemon (iki-genba) | Remote agent (transient while logged in) |

### Development Mode

- Human runs `ikigai` in a git worktree
- Terminal provides daemon functionality while running
- Local postgres, nginx, runit
- Agents connect to terminal's daemon socket

### Server Mode (iki-genba)

- ikigai-daemon keeps everything running
- Part of iki-genba PaaS (deno, runit, postgres, nginx)
- Humans connect remotely as transient agents
- Persistent Linux user identity, transient connection
- "SSH for agents" - log in, receive events, respond, log out

---

## Web Services Integration

Web services are separate from autonomous agents. They treat agents as async backend services:

```
┌──────────┐         ┌──────────┐         ┌──────────┐
│   Web    │  push   │  Agent   │  async  │  Agent   │
│ Service  │────────►│  Inbox   │────────►│ Process  │
│          │         │          │         │          │
│  (fast)  │         │ (queue)  │         │  (slow)  │
└──────────┘         └──────────┘         └──────────┘
     │                                          │
     │         poll/subscribe for result        │
     ◄──────────────────────────────────────────┘
```

- Web doesn't know or care it's talking to an agent
- Agents don't respond in web-service time frames
- Async event system pattern
- Results via database polling, webhooks, or similar

Agents can have tools that create/modify records that web services read. Web services can push input from web users into agent inboxes. All through the shared database.

---

## Worktrees and Agents

**One top-level agent per worktree.** Each worktree has exactly one top-level agent — the human or daemon working in that branch. Sub-agents spawned during a session work in the same worktree as their parent.

**Worktree-aware database.** All agents share one database, but records are tagged with their worktree. Agents in `rel-05` see `rel-05` data by default. Cross-worktree queries are possible when needed (e.g., ikigai terminal showing all worktrees).

**Git as transaction log.** Every conversation exchange with the LLM ends with a git commit. Uncommitted files are auto-committed. This creates a perfect correlation between git history and conversation history — you can roll back both code and context to any point.

---

## Shared World

All agents in a project share:
- **Database**: Messages, history, registry, state (worktree-aware)
- **Git worktree**: Files, code, configuration (one worktree per top-level agent)

Conflict resolution (multiple agents editing same file) is a software design problem. The human designer writes rules into agent code/config.

---

## Supervision

Simple approach:
- **Runit** manages agent processes
- Crashed agent gets restarted automatically
- On restart, replay unacked messages
- Heartbeat in main loop indicates liveness
- Stale heartbeat → mark as crashed → pending messages eventually return errors to senders

---

## Development Experience

Agents are TypeScript code in runit service directories:

```
.ikigai/sv/
├── code-reviewer/
│   ├── run              # runit script
│   └── main.ts          # imports from @ikigai/agent
├── news-digest/
│   ├── run
│   └── main.ts
└── ...
```

The `ikigai` package provides multiple modules:
- `ikigai/agent`: Agent class with registration, heartbeat, message loop
- `ikigai/platform`: Database access (history, pub/sub, queues), LLM integration
- Tool definition helpers

Agents discover the daemon by walking up the directory tree to find `.ikigai/daemon.sock`, similar to how git finds `.git/`. This works whether the agent runs from a worktree subdirectory or from the service directory.

Human codes at a high level of abstraction. Library handles plumbing.

Projects will likely start with built-in services:
- Web interface to monitor/list agents
- Registry viewer
- Log aggregation
- Other infrastructure

---

## General Purpose

Autonomous agents are not limited to coding tasks. They can:
- Review code and watch for commits
- Monitor logs and respond to alerts
- Digest news and provide summaries
- Trade stocks
- Manage robotic systems
- Provide customer service
- Any task that can be expressed as: receive input → think → respond

The system is a **general-purpose autonomous agent orchestration platform**.
