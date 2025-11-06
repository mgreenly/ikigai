# Architecture Decision Records

## Why WebSocket for Client-Server Communication?

**Decision**: Use WebSocket with JSON messages instead of HTTP/2 or gRPC.

**Rationale**:
- Bidirectional streaming needed for LLM responses and tool delegation
- Minimal framing overhead (2-6 bytes per message)
- Native browser support enables future web clients without protocol translation
- Simpler than gRPC for this use case (don't need protobuf complexity)
- libulfius provides robust WebSocket implementation

**Trade-offs**: WebSocket requires connection state management, but that's acceptable given our conversation-centric design.

---

## Why Start with OpenAI API Format?

**Decision**: Phase 1 uses OpenAI Chat Completions API format directly. Server acts as thin proxy that injects credentials.

**Rationale**:
- OpenAI was first to market with modern chat completion APIs
- Most widely adopted format in the ecosystem
- Well-documented and stable
- Provides good feature baseline (streaming, temperature, max_tokens, etc.)

This allows rapid Phase 1 implementation while establishing the foundation for provider abstraction in Phase 2.

---

## Why Superset API Approach?

**Decision**: Internal protocol will evolve to support the superset of all LLM provider features, not the common subset.

**Rationale**:
- **User choice**: Users can switch between models/providers without losing functionality
- **Feature exposure**: Advanced features (thinking tokens, extended context, etc.) are available when supported
- **Provider polyfill**: Individual provider adapters handle missing features through:
  - Ignoring unsupported parameters
  - Translating concepts (e.g., folding system prompt into messages for providers without native support)
  - Null operations where appropriate

**Alternative considered**: Common subset approach would artificially limit functionality to the least capable provider.

---

## Why PostgreSQL + Valkey?

**Decision**: Use PostgreSQL for persistent conversation storage and Valkey (Redis) for caching and ephemeral state.

**Rationale**:
- **PostgreSQL**: ACID guarantees for conversation history, rich query capabilities for future RAG retrieval, battle-tested
- **Valkey**: Fast key-value access for session state, WebSocket connection metadata, potential message queue for future scaling

**Trade-offs**: Operational complexity of two storage systems, but the separation of concerns is valuable as the system scales.

---

## Why Trust-Based Identity?

**Decision**: Client identity is `hostname@username` with no authentication in initial phases.

**Rationale**:
- Target use case is internal development tool
- Focus on functionality over security in early phases
- Reduces implementation complexity during prototyping

**Threat model**: System assumes trusted network and trusted users. Not suitable for public deployment without adding authentication layer.

**Future consideration**: Authentication can be added in later phase without breaking protocol structure (add auth handshake before first exchange).

---

## Why Client-Side Tool Execution?

**Decision**: All tools execute on the client, even those that proxy through server (web search, RAG).

**Rationale**:
- **File access**: Client needs access to local filesystem for reading/editing code
- **Shell commands**: Must run in user's actual environment with their permissions
- **Uniform interface**: Client tool handler doesn't need to know which tools proxy to server
- **Full trust model**: No sandboxing complexity—user explicitly runs this tool

**Trade-offs**: Requires client to be more than just a dumb terminal. But this matches the "local development environment" use case.

---

## Why Phased Implementation?

**Decision**: Break development into 6 incremental build phases. Phase 1 is purely architectural foundation, later phases add features.

**Rationale**:
- **Foundation-first approach**: Phase 1 builds the core architectural patterns (concurrency, memory management, error handling) that can't be retrofitted later. This is not an MVP - it's building the house's frame before adding rooms.
- **Risk reduction**: Validate the hardest architectural decisions (libulfius shutdown behavior, worker thread patterns) before building features on top.
- **Avoid costly refactoring**: You can't bolt on proper concurrency or memory management after the fact. Get it right in Phase 1, then build features that use these patterns.
- **Incremental feature complexity**: Once the foundation is solid, add storage, tools, providers, RAG one at a time in later phases.
- **Each phase builds on previous**: Phase 2 uses Phase 1's worker threads for multiple providers. Phase 3 uses them for database operations. Phase 6 uses them for tool execution.

**What makes Phase 1 different**: It's not trying to be useful to end users. It's establishing patterns. The "product" of Phase 1 is a proven architectural foundation, not a usable tool.

**Phase boundaries**: Chosen to separate architectural foundations (Phase 1) from feature additions (Phases 2-6). Once the foundation is built, features can be added incrementally without major refactoring.

---

## Why No Config Hot-Reload?

**Decision**: Configuration changes require server restart. No hot-reload mechanism.

**Rationale**:
- **Future config scope**: Config will control database connections, thread pools, feature flags, provider credentials, resource limits - essentially every aspect of server operation
- **Shutdown required anyway**: Changing most config values (listen address, database URL, worker threads) would require shutting down and restarting internal subsystems
- **Complexity not justified**: Hot-reload adds significant complexity (validation, rollback, partial application, race conditions) for minimal benefit
- **Server restart is fast**: With no persistent state in Phase 1-2, restart is < 1 second
- **Single-user localhost tool**: User can restart the server when needed without impacting others

**Alternative considered**: SIGHUP handler to reload config. Rejected because it would need to handle partial reload failures, validation errors, and concurrent request handling during reload.

**When this might change**: Never. If deployment model changes to multi-tenant hosted service, we'd use environment variables + container restart rather than hot-reload.

---

## Why Close WebSocket on Error?

**Decision**: Server sends `error` message and closes WebSocket. Client reconnects for new session.

**Rationale**:
- **Simplicity**: No error recovery state machine
- **Clean slate**: Each connection is fresh, no lingering error state
- **Fail fast**: Errors are explicit, not hidden
- **Phase 1 scope**: No conversation history to preserve

**Trade-offs**: Extra connection overhead on errors, but errors should be rare in normal operation.

---

## Why talloc for Memory Management?

**Decision**: Use talloc hierarchical allocator instead of manual malloc/free or custom arena.

**Rationale**:
- **Automatic cleanup**: Parent context frees all children
- **Lifecycle match**: Request/response cycles map to context hierarchies naturally
- **Debugging**: Built-in leak detection and memory tree reporting
- **Battle-tested**: Used in Samba, proven reliable
- **Available**: Already in Debian repos (libtalloc-dev)

**Trade-offs**: Dependency on external library, but the productivity gain outweighs this cost.

**Pattern**: Connection context → message context → parsing allocations

---

## Why Single-Request-Per-Connection Model?

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

---

## Why Mutex-Based Thread-Safe Logger?

**Decision**: Logger uses `pthread_mutex_t` to ensure atomic log line writes. Each log call acquires a global mutex before writing and releases it after flushing output.

**Rationale**:
- **Multi-threaded server**: Phase 1 uses multiple threads (libulfius WebSocket threads + worker threads per connection)
- **Prevent message interleaving**: Without synchronization, concurrent log calls produce garbled output (e.g., "INFO: ERR"Starting serverOR: Failed to connect")
- **Atomic writes required**: Each complete message (prefix + content + newline) must be written as a unit
- **Simple and effective**: Single global mutex is straightforward to implement and reason about
- **Acceptable performance**: Logging is not on critical path; mutex contention is negligible for typical log volumes

**Alternative considered**: `flockfile()`/`funlockfile()` (POSIX stream locking). Rejected because:
- Less explicit control over critical section
- Still requires pthread library
- No significant advantage over explicit mutex

**Implementation**:
```c
static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void ik_log_info(const char *fmt, ...) {
    pthread_mutex_lock(&ik_log_mutex);
    fprintf(stdout, "INFO: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);  // Ensure write completes before unlock
    pthread_mutex_unlock(&ik_log_mutex);
}
```

**Dependency impact**: Logger now depends on pthread (already required for worker threads, so no new external dependency).
