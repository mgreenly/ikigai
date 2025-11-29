# Vision

Ikigai is infrastructure you own.

A small team with well-orchestrated AI agents can have the awareness and reach of a much larger organization. Agents that watch for what matters to your goals, synthesize information you couldn't process alone, and act on your behalf when conditions are right. Whether you're building a business, pursuing research, or working toward any mission—agents extend what a small group of people can realistically pay attention to.

The platform handles the infrastructure so you can focus on the interesting problems: what should your agents watch for, what patterns matter, what actions should they take, how do they coordinate.

```
apt install ikigai
```

From there, you have a complete platform for autonomous agents. Your agents run on your servers, with direct access to the models you choose—your API keys, your usage, your costs. No middleman between you and the LLM.

---

## Platform as a Service

From the perspective of agents and webapps, Ikigai is a Platform as a Service. They're built for Ikigai—they import `@ikigai/platform` and consume its services:

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

This isn't a limitation—it's a deliberate choice that eliminates entire categories of complexity. Linux solved these problems decades ago; Ikigai leverages that work rather than reinventing it. Organizations with existing Linux infrastructure (LDAP, Active Directory via SSSD, centralized sudo policies) get integration for free.

---

## Target User

**Developers who want to build and operate agents they fully control.**

Small teams—typically 2-10 engineers—who:
- Have a mission and understand AI can extend their reach
- Want agents running reliably, 24/7, without babysitting
- Prefer to own their infrastructure rather than rent it
- Value opinionated tools that work out of the box

Not a low-code drag-and-drop builder. Not a chatbot framework. A platform for engineers who treat their agents as strategic capabilities, not just automations.

---

## Open-Source Ecosystem

The platform's future is shaped by its community. New runtime backends, agent templates, integrations, and tooling—contributed by developers who need them, available to everyone. The collective effort makes the platform more powerful than any single organization could achieve.

---

## Ambitious Scope

Ikigai aims to be the foundation for sophisticated autonomous AI systems. The goal isn't simplicity for its own sake—it's removing accidental complexity so developers can focus on the hard problems: what their agents should pay attention to, how they reason about what they find, how they coordinate, and when they should act.

Every autonomous agent system needs the same foundational pieces: task queues, message passing, identity management, secrets handling, deployment pipelines, process supervision, telemetry, and operational tooling. Today, developers rebuild these from scratch for each project, or cobble them together from disparate tools that weren't designed to work together.

Ikigai provides these pieces as a cohesive platform—opinionated choices that work well together out of the box. Start with one server. Most teams never need more. When you do, the architecture supports scaling out without fundamental changes.
