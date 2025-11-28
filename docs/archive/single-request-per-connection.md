# Archived Decision: Single Request Per Connection

**Note:** This decision document has been archived (2025-11-13) because the server/protocol architecture was removed in Phase 2.5. ikigai is now a desktop-only application that connects directly to LLM APIs.

The original decision is preserved below for historical reference.

---

# Why Single-Request-Per-Connection Model?

**Decision**: Each WebSocket connection handles one request at a time. Clients open multiple connections for concurrent operations.

**Rationale**:
- **Client simplicity**: From the client's perspective, communication is fully synchronous—send request, wait for complete response, done. No need for response queuing, correlation matching, or dispatch logic.
- **No interleaving**: Responses arrive on the connection that sent the request. Client doesn't need to multiplex or demultiplex messages.
- **Clean mental model**: One question per connection, one answer per connection. Easy to reason about.
- **Natural alignment**: Matches how OpenAI streaming works—one HTTP request streams until `[DONE]`. We expose this same pattern to clients.
- **Concurrent operations**: Client can still do multiple things at once by opening multiple connections (e.g., one for streaming LLM response, another for file operations).
- **Efficient enough**: Modern systems handle multiple TCP connections with negligible overhead. The simplicity gain justifies the connection overhead.

**Scenario**: Client asks OpenAI a question (slow, 10-30 sec streaming response). While response streams on connection 1, client opens connection 2 to read a file (fast, immediate response). Both operations proceed independently on separate connections.

**Alternative considered**: Single connection with concurrent request pipelining and interleaved responses. Rejected because:
- Client would need complex correlation matching (which response goes with which request?)
- Client would need response dispatch queues and concurrent handler threads
- Protocol would need request IDs in both directions
- Much harder to implement correctly on both sides

**Trade-offs**:
- **Pro**: Client implementation is simple—just open connection, send message, read response stream, close
- **Con**: Multiple connections have overhead (additional TCP state, handshakes)
- **Con**: Server must track multiple connections per client (but this is straightforward)

**Server-side implementation note**: While the client sees synchronous behavior, the server uses worker threads internally to enable abort semantics—if a client disconnects during a long-running OpenAI request, the server can immediately cancel the HTTP call rather than wasting resources. This internal complexity is hidden from the client via the single-request-per-connection abstraction. See phase-1-details.md for worker thread architecture details.

**Protocol perspective**: Each connection gets its own `session_id` for independent tracking. Correlation IDs are server-generated for logging and observability, allowing reconstruction of request timelines when multiple connections are active.
