# Observability

> Part of [Cross-Cutting Concerns](09-filesystem.md). All agents emit telemetry through the same system.

---

## Telemetry Flow

```
┌──────────┐     ┌──────────┐     ┌──────────┐
│  Agent   │────▶│ Runtime  │◀────│  Ikigai  │
│(emit)    │     │(store)   │     │(query)   │
└──────────┘     └──────────┘     └──────────┘
```

Agents emit telemetry to the runtime. Ikigai queries it for the developer.

---

## Telemetry Types

### Logs

Structured log entries:

```typescript
platform.log({
    level: "info",
    message: "Health check completed",
    endpoint: "https://api.example.com/health",
    latency_ms: 142,
    status: "healthy"
});
```

### Metrics

Numeric measurements:

```typescript
platform.metric("health_check_latency_ms", 142, {
    endpoint: "api.example.com"
});
```

### Events

Discrete occurrences:

```typescript
platform.event("alert_sent", {
    mailbox: "ops-alerts",
    severity: "warning",
    message: "API latency above threshold"
});
```

---

## Querying via Ikigai

```
> show logs for monitoring-agent last 1 hour

> show metrics health_check_latency_ms last 24 hours

> how many tasks did monitoring-agent complete today?

> show errors across all agents
```

Ikigai translates natural language queries into runtime queries against the telemetry store.

---

## External Integration

For organizations with existing observability infrastructure:

- Export telemetry to Prometheus (metrics endpoint)
- Forward logs to syslog/journald (already integrated via systemd)
- Push to external systems via a dedicated telemetry-export agent

The platform doesn't replace existing observability—it integrates with it.

---

**Next**: Ready for production? Continue to [Part VI: Production Operations](13-targets.md) for multi-server deployment, backup, and GitOps configuration.
