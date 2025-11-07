# Why Close WebSocket on Error?

**Decision**: Server sends `error` message and closes WebSocket. Client reconnects for new session.

**Rationale**:
- **Simplicity**: No error recovery state machine
- **Clean slate**: Each connection is fresh, no lingering error state
- **Fail fast**: Errors are explicit, not hidden
- **Phase 1 scope**: No conversation history to preserve

**Trade-offs**: Extra connection overhead on errors, but errors should be rare in normal operation.
