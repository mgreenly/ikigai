# Architecture Decision Records

This directory contains architecture decision records (ADRs) documenting key design choices for the project.

## Implemented Decisions

These decisions are reflected in the current codebase:

### Technical Patterns
- [talloc-memory-management.md](talloc-memory-management.md) - Why talloc for Memory Management
- [mutex-based-logger.md](mutex-based-logger.md) - Why Mutex-Based Thread-Safe Logger
- [link-seams-mocking.md](link-seams-mocking.md) - Why Link Seams for External Library Mocking
- [no-config-hot-reload.md](no-config-hot-reload.md) - Why No Config Hot-Reload

## Future Work Decisions

These decisions guide future development but are not yet implemented:

### Protocol & Communication
- [websocket-communication.md](websocket-communication.md) - Why WebSocket for Client-Server Communication
- [single-request-per-connection.md](single-request-per-connection.md) - Why Single-Request-Per-Connection Model
- [close-websocket-on-error.md](close-websocket-on-error.md) - Why Close WebSocket on Error

### LLM Provider Integration
- [openai-api-format.md](openai-api-format.md) - Why Start with OpenAI API Format
- [superset-api-approach.md](superset-api-approach.md) - Why Superset API Approach

### Infrastructure & Storage
- [postgresql-valkey.md](postgresql-valkey.md) - Why PostgreSQL + Valkey

### Data Representation & Serialization
- [json-yaml-projection.md](json-yaml-projection.md) - Why JSON-Based Projection with Dual Format Output

### Implementation Strategy
- [phased-implementation.md](phased-implementation.md) - Why Phased Implementation
- [client-side-tool-execution.md](client-side-tool-execution.md) - Why Client-Side Tool Execution
- [trust-based-identity.md](trust-based-identity.md) - Why Trust-Based Identity

---

Each ADR follows a consistent format:
- **Decision**: What was decided
- **Rationale**: Why this choice was made
- **Trade-offs**: Pros and cons considered
- **Alternatives considered**: Other options that were rejected
