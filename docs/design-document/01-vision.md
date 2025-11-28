# Vision

Ikigai is infrastructure you own.

Cloud platforms (AWS Bedrock, Azure AI, GCP Vertex) offer managed agent services but lock you into their ecosystems and pricing. SaaS providers offer simplified deployment but own your runtime. Ikigai takes a different path: open-source infrastructure that runs on any Linux system.

```
apt install ikigai
```

From there, you have a complete platform for autonomous agents—no cloud account, no API keys to a middleman, no per-agent fees. Your agents run on your servers, coordinated by your runtime, storing data in your databases.

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

**Power developers building sophisticated autonomous AI systems.**

Not a low-code drag-and-drop builder. Not a chatbot framework. A platform for engineers who want full control over their agent infrastructure.

---

## Open-Source Ecosystem

The platform's future is shaped by its community. New runtime backends, agent templates, integrations, and tooling—contributed by developers who need them, available to everyone. The collective effort makes the platform more powerful than any single organization could achieve.

---

## Ambitious Scope

Ikigai aims to be the foundation for the most sophisticated autonomous AI systems. The goal isn't simplicity for its own sake—it's removing accidental complexity so developers can focus on the hard problems: agent intelligence, multi-agent coordination, and system reliability.

Every autonomous agent system needs the same foundational pieces: task queues, message passing, identity management, secrets handling, deployment pipelines, process supervision, telemetry, and operational tooling. Today, developers rebuild these from scratch for each project, or cobble them together from disparate tools that weren't designed to work together. Ikigai provides these common pieces as a cohesive platform, battle-tested and ready to use. Developers start at the interesting problems—what their agents do, how they reason, how they coordinate—instead of spending months on infrastructure that adds no unique value to their system.
