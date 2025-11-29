# Pillar 1: Ikigai Terminal

> This is the first of [four pillars](02-architecture.md) that make up the Ikigai platform. The Terminal is your interface to everything else.

---

## Purpose

Ikigai Terminal is the developer's primary interface to the platform. It is a conversational coding agent implemented in C that understands the runtime environment, deployment model, and operational concerns. Unlike general-purpose coding assistants, Ikigai is purpose-built for this platform.

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

- Written in C for performance and stability as a long-running terminal application
- Direct terminal rendering with UTF-8 support
- PostgreSQL-backed conversation history with full-text search
- Multi-provider LLM integration (OpenAI, Anthropic, Google, X.AI)
- Local tool execution (file operations, shell commands, code analysis)
- SSH-based remote server operations using the developer's credentials (transparent to the developer)

---

**Next**: [Runtime System](06-runtime.md), the coordination infrastructure that agents use
