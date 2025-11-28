# Appendix

> **Part VII: Reference** — Terminology, open questions, and future directions.

---

## Terminology

To avoid ambiguity, this document uses these terms consistently:

| Term | Meaning |
|------|---------|
| **Runtime environment** | The broader platform infrastructure: PostgreSQL, systemd, Linux services, and coordination primitives |
| **Runtime language** | Deno/TypeScript—the language and execution engine for agent code |
| **Platform package** | The `@ikigai/platform` TypeScript package that agents import to access Ikigai systems (secrets, queues, mailboxes, telemetry) |
| **Ikigai Terminal** | The C application running on the developer's local machine |

---

## Open Design Questions

The following items require further specification as the platform is implemented:

- **Task queue multi-tenancy**: How does row-level security interact with shared queues where multiple agents may claim tasks?
- **Error handling and retry semantics**: Retry policies, dead letter queues, task timeout and abandonment behavior
- **Platform configuration format**: Structure and contents of `/etc/ikigai/ikigai.conf`
- **systemd unit template**: Specification for `ikigai-agent@.service` template used to generate agent service units

---

**Next**: [Future Considerations](17-future.md)
