# Platform Architecture

Ikigai consists of four integrated pillars designed to work together:

```
                              Developer
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 Pillar 1: Ikigai Terminal (C)                       │
│         Developer interface to the entire platform                  │
│    Create • Develop • Test • Deploy • Monitor • Operate             │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 Pillar 2: Runtime System                            │
│              Task Queues • Mailboxes • Telemetry                    │
│                 Agent Registry • Coordination                       │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│              Pillar 3: Autonomous Agents (Deno)                     │
│           Long-running TypeScript processes on systemd              │
│          Pull tasks • Execute work • Send messages                  │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                 Pillar 4: Web Portal (Nginx)                        │
│              Browser-based access to the platform                   │
│     Static webapps • Backend API • Session authentication           │
└─────────────────────────────────────────────────────────────────────┘
```

---

## The Four Pillars

| Pillar | Component | Purpose |
|--------|-----------|---------|
| 1 | [Ikigai Terminal](03-terminal.md) | Developer's conversational interface to the platform |
| 2 | [Runtime System](04-runtime.md) | Coordination infrastructure (queues, mailboxes, telemetry) |
| 3 | [Autonomous Agents](05-agents.md) | Long-running Deno/TypeScript processes |
| 4 | [Web Portal](06-web-portal.md) | Browser-based access to the platform |

Each pillar is documented in detail in its own section.
