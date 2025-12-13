# Autonomous Agents

## What is an Autonomous Agent?

An autonomous agent is a standalone TypeScript process that runs alongside the ikigai terminal. It shares the same database, the same messaging infrastructure, and the same identity model. The only difference between an autonomous agent and the ikigai terminal is who makes decisions: code or human.

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

## Communication

### Inboxes and Topics

Every agent has:
- **Default inbox** (mandatory): `inbox:{agent_id}` - always listening, cannot be disabled
- **Topic subscriptions** (optional): agent can subscribe to topics, receive copies of published messages

When a message arrives, the agent knows which subscription delivered it.

### Message Delivery

- **Synchronous send failure**: If the target inbox is full or agent is dead, `send()` fails immediately
- **Complete messages**: Messages are buffered complete, not streamed token-by-token
- **Fan-out for topics**: When publishing to a topic, each subscriber gets a copy

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
| name | Human-readable name |
| type | Agent type (enables spawning new instances) |
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

## Relationship to the Terminal

The ikigai terminal is just another agent with a human at the keyboard.

| Aspect | Terminal | Autonomous Agent |
|--------|----------|------------------|
| Decision maker | Human | Code |
| Identity | Has one | Has one |
| Inbox | Has one | Has one |
| History | Writes it | Writes it |
| Tools | Has them | Has them |

**Tool/command parity**: Everything an agent can do via tools, humans can do via /slash-commands.

The terminal has **privileged tools** for:
- Eavesdropping on any agent's history
- Sending messages to any agent's inbox
- Querying the registry
- Database internals

These are just tools. An autonomous agent could have them too if the designer chooses.

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

All three are variations in TypeScript code. The library supports:
- Blocking wait for next message
- Sleep for duration
- Both patterns combined

Agents can run indefinitely (days, weeks) or complete a task and terminate.

---

## Deployment Modes

| Mode | Always-on provided by | Human access |
|------|----------------------|--------------|
| Development | ikigai terminal | Direct, at keyboard |
| Server | ikigai-daemon | Remote agent (transient while logged in) |

### Development Mode

- Human runs `ikigai` in a git worktree
- Terminal provides always-on services
- Local postgres, nginx, runit

### Server Mode

- ikigai-daemon keeps everything running
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
│   └── main.ts          # imports from ikigai/agent
├── news-digest/
│   ├── run
│   └── main.ts
└── ...
```

The `ikigai` package provides multiple modules:
- `ikigai/agent`: Agent class with registration, heartbeat, message loop
- `ikigai/platform`: Database access (history, pub/sub, queues), LLM integration
- Tool definition helpers

Agents connect to the daemon via Unix socket at `../../daemon.sock` relative to their service directory.

Human codes at a high level of abstraction. Library handles plumbing.

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
