# Why PostgreSQL + Valkey?

**Decision**: Use PostgreSQL for persistent conversation storage and Valkey (Redis) for caching and ephemeral state.

**Rationale**:
- **PostgreSQL**: ACID guarantees for conversation history, rich query capabilities for future RAG retrieval, battle-tested
- **Valkey**: Fast key-value access for session state, WebSocket connection metadata, potential message queue for future scaling

**Trade-offs**: Operational complexity of two storage systems, but the separation of concerns is valuable as the system scales.
