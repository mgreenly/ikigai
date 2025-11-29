# Vision

Ikigai is infrastructure you own.

A small team creating intelligent agents can have the effectiveness of a much larger group. Developers build agents that monitor what matters, process information at scales humans can't sustain, and decide when to act.

Ikigai handles the infrastructure so you can focus on the development of your agents.

```
apt install ikigai
```

From there, you have a complete platform for autonomous agents. Your agents run on your servers, with direct access to the models you choose: your API keys, your usage, your costs. No middleman between you and the LLM.

---

## One Install, Complete Platform

That single command gives you task queues, mailboxes, pub/sub, storage, caching, telemetry, and process management. No separate message broker, no cache cluster, no container orchestrator. PostgreSQL and systemd handle what other stacks spread across half a dozen services.

You can add specialized backends later if you need them. But the defaults handle everything, and they work together because they were chosen to work together.

---

## Platform as a Service

From the perspective of agents and webapps, Ikigai is a Platform as a Service. They're built for Ikigai. They import `@ikigai/platform` and consume its services:

- **Queues**: Claim tasks, complete work
- **Mailboxes**: Point-to-point messaging
- **Pub/Sub**: Broadcast to subscribers
- **Storage**: Persistent data
- **Cache**: Fast access to hot data
- **Telemetry**: Logs, metrics, events

Agents don't care whether their queue is PostgreSQL or Redis. They use the platform API; the infrastructure is configuration. This separation means:

1. **Agents stay simple**: No infrastructure code, just business logic
2. **Operations stay flexible**: Swap backends without touching agent code
3. **Defaults work**: PostgreSQL handles everything out of the box

---

## Linux is the Platform

Rather than building custom systems for identity, authorization, secrets, process management, and service orchestration, Ikigai uses Linux itself:

- Users are Linux users, managed through PAM
- Authorization is file permissions and sudo
- Secrets are files with restricted ownership
- Agents run as systemd services
- Logs flow through journald

This isn't a limitation; it's a deliberate choice that eliminates entire categories of complexity. Linux solved these problems decades ago; Ikigai leverages that work rather than reinventing it. Organizations with existing Linux infrastructure (LDAP, Active Directory via SSSD, centralized sudo policies) get integration for free.

---

## Omakase

David Heinemeier Hansson described Ruby on Rails as "The Omakase Stack", a cohesive set of choices made by the framework authors, like a chef's tasting menu where someone has already figured out what works well together. You can substitute ingredients, but the menu is designed as a whole.

Ikigai follows this philosophy. PostgreSQL for coordination. Systemd for process management. PAM for identity. Journald for logs. These aren't arbitrary defaults; they're choices that compose well on Linux. Omarchy, DHH's Linux desktop project, embodies the same idea: strong defaults that let you focus on your actual work instead of endless configuration.

You can swap pieces when you have specific needs. But the defaults are the defaults because they work together.

---

## Target User

**Developers who want to build and operate agents they fully control.**

Small teams (typically 2-10 engineers) who:
- Have a mission and understand AI can extend their reach
- Want agents running reliably, 24/7, without babysitting
- Prefer to own their infrastructure rather than rent it
- Value opinionated tools that work out of the box

Not a low-code drag-and-drop builder. Not a chatbot framework. A platform for engineers who treat their agents as strategic capabilities, not just automations.

---

## Open-Source Ecosystem

The platform's future is shaped by its community. New runtime backends, agent templates, integrations, and tooling, all contributed by developers who need them, available to everyone. The collective effort makes the platform more powerful than any single organization could achieve.

---

## Ambitious Scope

Ikigai aims to be the foundation for sophisticated autonomous AI systems. The goal isn't simplicity for its own sake; it's removing accidental complexity so developers can focus on the hard problems: what their agents should pay attention to, how they reason about what they find, how they coordinate, and when they should act.

Every autonomous agent system needs the same foundational pieces: task queues, message passing, identity management, secrets handling, deployment pipelines, process supervision, telemetry, and operational tooling. Today, developers rebuild these from scratch for each project, or cobble them together from disparate tools that weren't designed to work together.

Ikigai provides these pieces as a cohesive platform, opinionated choices that work well together out of the box.

---

## Vertical First

Start with one server. A modern Linux machine with fast storage and enough RAM can handle hundreds of concurrent agents. On a single box, there are no network partitions, no distributed consensus, no eventual consistency. Just processes talking through local sockets and a shared database.

Most teams never need more than this. When you do, the architecture supports scaling out without fundamental changes. But preserve the simplicity of a single server as long as you can. It makes everything easier to operate, debug, and reason about.
