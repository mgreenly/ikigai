# Pillar 3: Autonomous Agents

> This is the third of [four pillars](02-architecture.md). Agents are what you build, long-running processes that use the [Runtime](06-runtime.md) to coordinate. Agents run in [iki-genba](../iki-genba.md), the PaaS for external agents.

---

## Purpose

Autonomous agents are the core of what you build with Ikigai. They are long-running TypeScript processes that watch for what matters, synthesize information, and act when conditions are right, operating continuously without human intervention except when encountering genuine uncertainty.

Agents consume runtime services for coordination (queues, mailboxes) and for intelligence (LLM access via the [Ikigai Daemon](06-runtime.md#ikigai-daemon)). They're platform-native citizens with access to the same capabilities developers use through the Terminal.

---

## Runtime Environment

**Deno** is the execution runtime for agents:

- Native TypeScript execution (no build step)
- URL-based imports with lockfile (`deno.lock`)
- Explicit permission model (`--allow-net`, `--allow-read`, etc.)
- Single binary installation
- Built-in tooling (formatter, linter, test runner)

**systemd** manages agent processes in production:

- Automatic restart on failure
- Resource limits (CPU, memory)
- Logging integration with journald
- Dependency ordering (start after PostgreSQL)
- Graceful shutdown via SIGTERM with configurable timeout

---

## Agent Structure

Every agent follows a standard structure:

```
monitoring-agent/
├── manifest.json       # Metadata and configuration
├── deno.json           # Deno configuration
├── deno.lock           # Locked dependencies
├── run.ts              # Entry point
└── src/                # Agent implementation
    ├── handlers/
    ├── lib/
    └── types/
```

### manifest.json

```json
{
    "name": "monitoring-agent",
    "version": "1.2.3",
    "description": "Monitors API health and alerts on failures",

    "runtime": {
        "engine": "deno",
        "version": ">=2.0.0",
        "permissions": [
            "--allow-net",
            "--allow-env",
            "--allow-read=/etc/ikigai"
        ]
    },

    "entry": "run.ts",

    "platform": {
        "queues": ["monitoring-tasks"],
        "mailboxes": ["ops-alerts"]
    },

    "llm": {
        "models": ["haiku", "sonnet"],
        "max_tokens_per_request": 4096,
        "max_tokens_per_hour": 100000,
        "platform_tools": ["query_telemetry", "send_message"],
        "allow_custom_tools": true
    },

    "lifecycle": {
        "shutdown_timeout_seconds": 30
    }
}
```

The manifest declares everything the platform needs to run the agent correctly: Deno permissions, queue subscriptions, mailbox access, and LLM capabilities.

### Health Checking

Agent health is verified using an Erlang-inspired supervision model: the platform sends a health check message to the agent's command mailbox and expects a response within a timeout. This message-based approach works uniformly across all agents without requiring them to expose HTTP endpoints.

---

## Agent Lifecycle

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Write   │────▶│   Test   │────▶│  Deploy  │────▶│   Run    │
│  (local) │     │  (local) │     │  (CI/CD) │     │(systemd) │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                                                         │
                      ┌──────────────────────────────────┘
                      ▼
               ┌──────────┐     ┌──────────┐
               │ Monitor  │◀───▶│ Operate  │
               │(telemetry│     │(start/   │
               │  /logs)  │     │stop/roll │
               └──────────┘     │  back)   │
                                └──────────┘
```

**Development**: Developer writes agent locally in `~/project/agents.d/monitoring-agent/`. Same structure as production.

**Testing**: Developer runs `deno task test` or asks Ikigai to run the agent locally. On Linux, the agent connects to natively installed runtime services (PostgreSQL, etc.), the same tools used in production. Ikigai is a Linux-first platform; native Linux development is the official path.

**Deployment**: Developer says "deploy monitoring-agent v1.2.0". Ikigai:
1. On first deploy: creates Linux user, PostgreSQL role, directories
2. Clones tag to `/opt/ikigai/agents.d/monitoring-agent/v1.2.0/`
3. Updates symlink: `current -> v1.2.0`
4. Restarts service (graceful shutdown of old version)
5. Verifies health check passes

**Operation**: Agent runs continuously. Ikigai can query status, view logs, inspect metrics, stop/start/restart, or rollback to previous version.

---

## Event Loop Architecture

Autonomous agents are event-driven, not request-driven:

```typescript
// run.ts - simplified agent structure
import { Platform } from "ikigai/platform";

const platform = await Platform.connect();  // connects to ../../daemon.sock

// Register signal handlers
platform.onShutdown(async () => {
    await gracefulShutdown();
});

// Main event loop
while (running) {
    const signals = await platform.awaitAny([
        platform.queue("monitoring-tasks"),
        platform.timer(5 * 60 * 1000),  // 5 minute heartbeat
        platform.mailbox("monitoring-commands"),
    ]);

    for (const signal of signals) {
        switch (signal.type) {
            case "task":
                await handleTask(signal.task);
                break;
            case "timer":
                await performScheduledWork();
                break;
            case "message":
                await handleCommand(signal.message);
                break;
        }
    }
}
```

This architecture enables true autonomy: agents respond to multiple signal types, work continuously, and only involve humans when necessary.

---

## LLM Access

Agents access LLM capabilities through the [Ikigai Daemon](06-runtime.md#ikigai-daemon), using the same `ikigai/platform` module they use for queues and mailboxes.

### Basic Usage

```typescript
import { Platform } from "ikigai/platform";

const platform = await Platform.connect();

// Simple prompt
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [
        { role: "user", content: "Summarize this error log..." }
    ],
});

console.log(response.message.content);
```

### Using Platform Tools

Agents can give the LLM access to platform operations:

```typescript
const response = await platform.prompt({
    model: "sonnet-4.5",
    messages: [{
        role: "user",
        content: `I found these anomalies: ${anomalies}.
                  Check recent telemetry and decide if we should alert.`
    }],
    tools: {
        platform: ["query_telemetry", "send_message"],
    },
});

// The LLM might:
// 1. Call query_telemetry to check error rates
// 2. Decide based on the data
// 3. Call send_message to alert if needed
```

The daemon executes tool calls with proper permissions and transactions. Agents express intent; the platform handles execution.

### Conversation Threading

For multi-turn reasoning:

```typescript
const thread = await platform.threads.create({
    context: "Analyzing support ticket batch"
});

// First pass: categorize
await platform.prompt({
    thread: thread.id,
    messages: [{ role: "user", content: "Categorize these tickets..." }],
});

// Second pass: prioritize (has context from first pass)
const response = await platform.prompt({
    thread: thread.id,
    messages: [{ role: "user", content: "Now prioritize by urgency." }],
});
```

### Memory and Context Control

Agents control their own context window using the same primitives available to developers in the Terminal. The API provides two layers: high-level operations for common tasks and low-level access for precise control.

```typescript
// High-level: mark and rewind (like Terminal's /mark and /rewind)
const checkpoint = await platform.context.mark("before-experiment");

// ... try something that might pollute context ...

await platform.context.rewind(checkpoint);  // Roll back if needed

// Exclude specific exchanges from future context
await platform.context.exclude(response.id);

// Low-level: direct StoredAsset management
await platform.storedAssets.create({
    label: "task_state",
    value: JSON.stringify({ processed: 0, errors: [] }),
});

// Update as work progresses
await platform.storedAssets.update("task_state", {
    value: JSON.stringify({ processed: 47, errors: ["item-23"] }),
});

// LLM can also manage its own memory via tools
await platform.prompt({
    messages: [{ role: "user", content: "Update your working memory with what you learned." }],
    tools: {
        platform: ["memory_read", "memory_write"],
    },
});
```

The full memory and context API is documented in the [Runtime System](06-runtime.md#memory-and-context).

### Why Platform-Mediated LLM Access?

Agents could call LLM providers directly. Using the daemon instead provides:

1. **Unified observability**: All LLM calls (developer + agent) in one place
2. **Centralized keys**: API keys stored once, not per-agent
3. **Platform tools**: LLMs can operate the platform (create tasks, send messages)
4. **Permission enforcement**: Limits on models, tokens, tools per agent
5. **Cost tracking**: Attribute usage to specific agents and tasks
6. **Conversation management**: Threading, history, context optimization handled centrally

The same `libikigai` that powers the Terminal powers agent LLM access. Improvements benefit both.

---

## Graceful Shutdown

Agents must handle SIGTERM gracefully. During deployment or restart:

1. **SIGTERM sent**: systemd signals the agent to stop
2. **Stop accepting work**: Agent stops pulling new tasks from queues
3. **Finish current work**: Complete in-progress task (don't abandon mid-task)
4. **Clean up**: Close connections, flush buffers
5. **Exit**: Process terminates cleanly
6. **Grace period expires**: If still running, systemd sends SIGKILL

The grace period comes from `manifest.json`:

```json
{
    "lifecycle": {
        "shutdown_timeout_seconds": 30
    }
}
```

### Well-Behaved Agent Pattern

```typescript
import { Platform } from "ikigai/platform";

const platform = await Platform.connect();
let running = true;
let processing = false;

// Handle SIGTERM
Deno.addSignalListener("SIGTERM", () => {
    console.log("SIGTERM received, shutting down gracefully...");
    running = false;
    // If not processing, exit immediately
    if (!processing) {
        Deno.exit(0);
    }
    // Otherwise, let current task finish
});

// Main event loop
while (running) {
    const task = await platform.queue("my-tasks").claim({
        timeout: 5000  // Short timeout so we check `running` frequently
    });

    if (task) {
        processing = true;
        try {
            await handleTask(task);
            await task.complete();
        } catch (err) {
            await task.fail(err.message);
        } finally {
            processing = false;
        }

        // Check if shutdown requested between tasks
        if (!running) {
            console.log("Shutdown: finished current task, exiting");
            Deno.exit(0);
        }
    }
}

console.log("Shutdown: no active task, exiting");
Deno.exit(0);
```

### Key Requirements

| Requirement | Why |
|-------------|-----|
| Handle SIGTERM | systemd sends this on stop/restart |
| Stop accepting new work | Don't claim tasks you won't finish |
| Finish current task | Abandoning mid-task corrupts state |
| Exit within timeout | Avoid SIGKILL (ungraceful termination) |
| Short queue timeouts | Check shutdown flag frequently |

### systemd Unit Template

The platform generates systemd units with proper shutdown handling:

```ini
[Service]
Type=simple
ExecStart=/opt/ikigai/lib/deno/current/deno run \
    --allow-net --allow-env --allow-read=/etc/ikigai \
    /opt/ikigai/agents.d/monitoring-agent/current/run.ts

# Graceful shutdown
KillSignal=SIGTERM
TimeoutStopSec=30        # From manifest.lifecycle.shutdown_timeout_seconds
FinalKillSignal=SIGKILL  # Force kill if timeout exceeded

# Restart policy
Restart=always
RestartSec=5
```

---

**Next**: [Web Portal](08-web-portal.md), browser-based access to the platform
