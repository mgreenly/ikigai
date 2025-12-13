# Iki-genba

Iki-genba is a PaaS for running autonomous agents. It provides the runtime where code-driven agents live and operate independently of human interaction.

---

## The Name

**Iki-genba** (生き現場) combines two Japanese words:

- **生き (iki)** — living, alive
- **現場 (genba)** — the actual place; where real work happens

Together: "the living site" or "where life actually happens."

In lean manufacturing, *genba* refers to the shop floor — the place you must go to understand what's really going on. Iki-genba is where your agents do their continuous work.

---

## Internal and External Agents

Ikigai supports two modes of agent operation:

**Internal agents** are driven by a human at the terminal. You start a conversation, spawn agents to help, and guide the work. When you close the terminal, that tree of work concludes. The human is always at the root.

**External agents** are driven by code. They can run continuously in iki-genba, responding to events, schedules, or messages from other agents. No human needs to be present. The code is at the root.

Both types share the same database, the same messaging system, and the same identity model. An internal agent spawned from your terminal can send a message to an external agent running in iki-genba. They coordinate through the shared system.

---

## Why a PaaS

Autonomous agents need somewhere to run. You could cobble together your own stack — a process supervisor, a way to manage services, a database, a reverse proxy. Or you can use iki-genba.

Iki-genba provides:
- A TypeScript runtime (Deno)
- Process supervision (runit)
- A coordination database (PostgreSQL)
- HTTP routing (nginx)

The pieces are standard and boring. The value is that they're already wired together and ikigai understands them. You write your agent, drop it in the right place, and it runs.

---

## Relationship to Ikigai

Ikigai is the terminal where humans direct agent work. Iki-genba is the runtime where code directs agent work.

**Ikigai without iki-genba** is a capable interactive agent system — similar to other agentic coding tools, but with ikigai's particular approach to history, messaging, and tools.

**Ikigai with iki-genba** adds persistent, code-driven agents. You can deploy agents that watch for events, run on schedules, or coordinate long-running work without human involvement.

Ikigai has tooling to deploy, manage, and communicate with agents running in iki-genba. The terminal understands the PaaS.

---

## Deployment Options

Iki-genba runs wherever you need it:

**On your laptop** — for development and personal automation. Your agents run locally, coordinating through a local database.

**On a server** — for always-on agents. The same structure, just running as system services instead of local processes.

The configuration is the same in both cases. What works locally works in production.

---

## Optional, Not Required

Iki-genba is optional. If you only want interactive agent work through the terminal, you don't need it. Ikigai works fine on its own.

But if you want agents that run independently — watching logs, processing queues, responding to webhooks, running scheduled tasks — iki-genba gives them a place to live.
