# Development Workflow

This section walks through the development experience: creating an agent, running it locally, testing it, and deploying it to a server.

---

## Creating Your First Agent

Start a conversation:

```
> create an agent that monitors my API health every 5 minutes
  and sends alerts to an ops mailbox if anything is down

Creating monitoring-agent...

  agents.d/monitoring-agent/
  ├── manifest.json       # Agent metadata and permissions
  ├── deno.json           # Deno configuration
  ├── deno.lock           # Locked dependencies
  ├── run.ts              # Entry point
  └── src/
      └── handlers.ts     # Your business logic goes here

The agent will:
  • Poll https://api.example.com/health every 5 minutes
  • Send messages to the "ops-alerts" mailbox on failures
  • Log health check results to telemetry

Want me to explain the code, or shall we run it?
```

Ikigai generates working code following platform conventions. You can immediately run it, or ask questions about how it works.

---

## Project Structure

The developer's local environment mirrors production:

```
~/projects/my-agent/
├── agents.d/
│   └── monitoring-agent/
│       ├── manifest.json
│       ├── deno.json
│       ├── deno.lock
│       ├── run.ts
│       └── src/
└── docker-compose.yml          # Local runtime services
```

The `agents.d/` directory structure is intentional—it matches where agents live on production servers (`/opt/ikigai/agents.d/`). Same structure, same conventions, same code.

---

## Running Locally

**Start local runtime** (PostgreSQL and other services):

```bash
docker compose up -d
```

**Run the agent**:

```
> run monitoring-agent

Starting monitoring-agent locally...

  Runtime: local PostgreSQL (localhost:5432)
  Queues: monitoring-tasks
  Mailboxes: ops-alerts

[monitoring-agent] Health check: https://api.example.com/health
[monitoring-agent] Status: healthy (142ms)
[monitoring-agent] Next check in 5 minutes...
```

Ikigai executes `deno run` with the permissions from `manifest.json` and environment variables pointing to the local runtime.

**Iterating**:

```
> the health check should also check the /ready endpoint

Updating monitoring-agent...

  Modified: src/handlers.ts
  - Added /ready endpoint check
  - Both endpoints must pass for healthy status

Restarting agent...

[monitoring-agent] Health check: https://api.example.com/health ✓
[monitoring-agent] Ready check: https://api.example.com/ready ✓
[monitoring-agent] Status: healthy
```

Code changes take effect immediately—no build step, no restart dance.

---

## Testing

### Unit Tests

Standard Deno testing for pure functions and logic:

```bash
deno test
```

```typescript
// src/handlers_test.ts
import { assertEquals } from "jsr:@std/assert";
import { parseHealthResponse } from "./handlers.ts";

Deno.test("parseHealthResponse extracts status", () => {
    const response = { status: "healthy", latency_ms: 142 };
    assertEquals(parseHealthResponse(response).healthy, true);
});
```

### Integration Tests

Test the agent against a real runtime with fixtures:

```
> test monitoring-agent

Setting up test environment...
  Creating test queue: monitoring-tasks
  Creating test mailbox: ops-alerts
  Inserting test task...

Running agent...
  [monitoring-agent] Claimed task from monitoring-tasks
  [monitoring-agent] Executing health check...
  [monitoring-agent] Task completed

Verifying results...
  ✓ Task marked complete
  ✓ Telemetry logged
  ✓ No alerts sent (healthy response)

Cleaning up... done

All tests passed.
```

Ikigai sets up fixtures, runs the agent, verifies expected behavior, and tears down.

---

## Multi-Agent Development

When building systems where agents communicate:

```
~/projects/my-system/
├── agents.d/
│   ├── monitoring-agent/
│   └── alerting-agent/
└── docker-compose.yml
```

Run them together:

```
> run monitoring-agent and alerting-agent

Starting monitoring-agent...
Starting alerting-agent...

  Both agents share local runtime
  Mailbox communication enabled

[monitoring-agent] Health check failed: connection timeout
[monitoring-agent] Sending alert to ops-alerts mailbox
[alerting-agent] Received alert from monitoring-agent
[alerting-agent] Sending notification to #ops channel
```

The agents communicate through the local runtime just as they would in production.

---

## Deploying

When the agent is ready:

```
> deploy monitoring-agent

Target: local (no remote target selected)

To deploy to a server, first add a target:
  > add target staging

Or deploy locally for testing:
  > deploy monitoring-agent --local
```

After adding a target:

```
> target staging
Switched to staging (staging.example.com)

[staging] > deploy monitoring-agent

Deploying monitoring-agent v1.0.0 to staging...

  Creating Linux user... done
  Creating PostgreSQL role... done
  Cloning repository... done
  Generating systemd unit... done
  Starting service... done
  Health check... ✓

monitoring-agent is running on staging.

[staging] > show logs monitoring-agent

[monitoring-agent] Started
[monitoring-agent] Health check: https://api.example.com/health
[monitoring-agent] Status: healthy (89ms)
```

See [Remote Targets](13-targets.md) for more on managing deployment destinations.

---

## The Development Loop

The typical workflow:

1. **Create**: Describe what you want, Ikigai generates code
2. **Run**: Execute locally against real runtime services
3. **Iterate**: Make changes, see results immediately
4. **Test**: Unit tests for logic, integration tests for behavior
5. **Deploy**: Push to staging, then production
6. **Monitor**: Query telemetry, check health, view logs

All through conversation. The infrastructure complexity exists but stays out of your way.
