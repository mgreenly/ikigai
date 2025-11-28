# Why No Config Hot-Reload?

**Decision**: Configuration changes require server restart. No hot-reload mechanism.

**Rationale**:
- **Future config scope**: Config will control database connections, thread pools, feature flags, provider credentials, resource limits - essentially every aspect of server operation
- **Shutdown required anyway**: Changing most config values (listen address, database URL, worker threads) would require shutting down and restarting internal subsystems
- **Complexity not justified**: Hot-reload adds significant complexity (validation, rollback, partial application, race conditions) for minimal benefit
- **Server restart is fast**: With no persistent state in Phase 1-2, restart is < 1 second
- **Single-user localhost tool**: User can restart the server when needed without impacting others

**Alternative considered**: SIGHUP handler to reload config. Rejected because it would need to handle partial reload failures, validation errors, and concurrent request handling during reload.

**When this might change**: Never. If deployment model changes to multi-tenant hosted service, we'd use environment variables + container restart rather than hot-reload.
