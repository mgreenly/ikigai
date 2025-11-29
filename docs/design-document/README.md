# Ikigai Design Document

## A Developer Platform for Autonomous Agents

---

## Summary

Ikigai is a platform for building, deploying, and operating autonomous AI agents on Linux. A small team with well-orchestrated agents can have the awareness and reach of a much larger organization—watching for things that matter, synthesizing information, and acting when conditions are right.

The platform handles the infrastructure (queues, messaging, telemetry, deployment, identity) so you can focus on what your agents should do, what patterns matter, and how they coordinate. Direct access to the models you choose—your API keys, your usage, your costs—with no middleman between you and the LLM.

The platform philosophy: **use Linux, don't abstract it.** Identity is Linux users via PAM. Secrets are files with permissions. Agents run as systemd services. Start with one server—most people never need more.

---

## Core Principles

1. **Use Linux, don't abstract it**: Identity is PAM, secrets are files, processes are systemd services. Linux solved these problems decades ago—Ikigai leverages that work rather than reinventing it.

2. **Opinionated defaults, escape hatches when needed**: PostgreSQL handles queues, messaging, storage, caching, and telemetry out of the box. One dependency, zero configuration. When specific needs arise, swap individual services to specialized backends without changing agent code.

3. **Developer experience through conversation**: Ikigai Terminal understands the entire platform—how to write agents, deploy them, and operate them. Describe what you want; the complexity is handled automatically.

4. **Direct model access**: Your API keys, your chosen providers, your costs. No platform sitting between you and the LLMs your agents use.

---

## Table of Contents

### Part I: Why Ikigai Exists
1. [Vision](01-vision.md) — Platform philosophy and target users

### Part II: Platform Overview
2. [Architecture](02-architecture.md) — The four pillars
3. [First-Run Experience](03-first-run.md) — From install to first agent

### Part III: Building Your First Agent
4. [Development Workflow](04-development.md) — Local development and testing

### Part IV: The Four Pillars
5. [Ikigai Terminal](05-terminal.md) — Developer interface (Pillar 1)
6. [Runtime System](06-runtime.md) — Coordination infrastructure (Pillar 2)
7. [Autonomous Agents](07-agents.md) — Agent execution environment (Pillar 3)
8. [Web Portal](08-web-portal.md) — Browser-based access (Pillar 4)

### Part V: Cross-Cutting Concerns
9. [Filesystem Layout](09-filesystem.md) — FHS-compliant directory structure
10. [Identity and Security](10-security.md) — Linux-native security model
11. [Deployment](11-deployment.md) — Git-based versioned deployments
12. [Observability](12-observability.md) — Telemetry, logging, and monitoring

### Part VI: Production Operations
13. [Remote Targets](13-targets.md) — Managing multiple servers
14. [Backup and Recovery](14-backup.md) — Disaster recovery procedures
15. [GitOps Configuration](15-gitops.md) — Configuration as code

### Part VII: Reference
16. [Appendix](16-appendix.md) — Terminology and open design questions
17. [Future Considerations](17-future.md) — Multi-server, RAG, additional interfaces
