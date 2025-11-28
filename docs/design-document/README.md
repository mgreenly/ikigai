# Ikigai Design Document

## A Developer Platform for Autonomous Agents

---

## Executive Summary

Ikigai is a platform for building, deploying, and operating autonomous AI agents on Linux. It combines a developer-focused coding agent, a runtime system providing coordination primitives, and a standardized agent execution environment into an integrated platform where the complexity of infrastructure, identity, and coordination is handled automatically.

The platform philosophy: **use Linux, don't abstract it.** Identity is Linux users via PAM. Secrets are files with permissions. Agents run as systemd services. The runtime system provides task queues, mailboxes, and telemetry. The developer interacts through a conversational terminal interface that understands the entire platform, from writing code to deploying to production to monitoring runtime behavior.

---

## Core Principles

1. **Use Linux, don't abstract it**: Identity is PAM, secrets are files, processes are systemd services. The platform leverages decades of Linux infrastructure rather than replacing it.

2. **Platform services with configurable backends**: Task queues, mailboxes, storage, caching, and telemetry are platform services that agents consume through `@ikigai/platform`. The default configuration uses PostgreSQL for all services—simple, proven, and sufficient for most deployments. When specific needs arise, individual services can be reconfigured to use specialized backends (Redis for caching, NATS for messaging, etc.) without changing agent code.

3. **Developer experience through conversation**: Ikigai Terminal understands the entire platform. Developers describe what they want; the complexity of deployment, identity, and operations is handled automatically.

---

## Quick Start

```
apt install ikigai
```

From there, you have a complete platform for autonomous agents—no cloud account, no API keys to a middleman, no per-agent fees. Your agents run on your servers, coordinated by your runtime, storing data in your databases.

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
