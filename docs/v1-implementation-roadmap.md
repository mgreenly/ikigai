# v1.0 Implementation Roadmap

This document outlines the phase-by-phase implementation plan for ikigai v1.0, showing dependencies and deliverables for each phase.

## Implementation Sequence

Based on dependency analysis, here's the recommended implementation sequence:

---

## Phase 1: LLM HTTP Client

**Dependencies:** None (uses existing REPL foundation)

**Implementation Tasks:**
1. Add libcurl dependency to Makefile
2. Implement `ik_llm_client_t` module (src/llm_client.c)
3. OpenAI API integration with blocking streaming
4. Parse SSE format (Server-Sent Events) from OpenAI
5. Extract JSON chunks with yyjson
6. Accumulate streaming response in buffer
7. Display chunks in scrollback as they arrive
8. Store session messages in `ik_repl_ctx_t` (memory only)
9. Build API requests from session messages array

**Technical Details:**
- Blocking HTTP streaming (Option A from LLM integration doc)
- User frozen during response (acceptable for Phase 1)
- Simple callback-based chunk handling
- Messages stored in memory, lost on exit

**Deliverable:** Working AI chat with streaming responses. Messages lost on exit (no persistence yet).

**Testing:**
- Unit tests for SSE parsing
- Unit tests for JSON extraction
- Mock HTTP responses
- Integration test: user message → LLM response → display

---

## Phase 2: Basic Command Infrastructure

**Dependencies:** No hard dependencies (can run in parallel with Phase 1)

**Implementation Tasks:**
1. Create `src/commands.c` with command registry
2. Implement `/help` (auto-generated from registry)
3. Implement `/quit` (graceful exit)
4. Implement `/clear` (clear scrollback, session messages, and marks)
5. Implement `/mark [LABEL]` (create checkpoint for rollback)
6. Implement `/rewind [LABEL]` (rollback to checkpoint)
7. Move `/pp` into registry
8. Create `src/message_format.c` for message decorations

**Technical Details:**
- Command registry with function pointers
- Self-documenting (descriptions in registry)
- Each command handler is independent function
- Marks stored in-memory only (Phase 2)

**Deliverable:** Structured command system with context management (/clear, /mark, /rewind) and /help.

**Testing:**
- Unit test each command handler
- Test mark/rewind with mock messages
- Test /clear removes context but not from DB (when DB added)
- Test /help generates correct output

---

## Phase 3: Database Integration

**Dependencies:** Depends on Phase 1 for usefulness (needs messages to persist)

**Implementation Tasks:**
1. Add libpq dependency to Makefile
2. Implement `ik_db_ctx_t` module (src/db.c)
3. Create PostgreSQL schema (sessions and messages tables)
4. Add session tracking to REPL (create session on init, end on cleanup)
5. Persist messages on send/receive (INSERT after user input and LLM response)
6. Add `current_session_id` to `ik_repl_ctx_t`
7. Update config to include `db_connection_string`
8. Implement core database query functions (for tool use)
9. Add PostgreSQL full-text search index (GIN on messages.content)
10. Update /mark and /rewind to persist to database

**Technical Details:**
- Immediate persistence (synchronous INSERTs)
- Session created on app launch, ended on exit
- Messages reference current session
- Database failure doesn't block LLM interaction
- Mark/rewind operations persisted for replay

**Deliverable:** Permanent message storage, sessions tracked, full conversation history preserved.

**Testing:**
- Unit tests for database operations
- Test session lifecycle
- Test message persistence
- Test full-text search
- Test error handling (connection lost)
- Integration: complete conversation persisted and recoverable

**Note on Database Access:** Database access will primarily be performed by the LLM through tool use (Phase 5). The `/clear` command remains essential for user control of context.

---

## Phase 4: Interactive Streaming

**Dependencies:** Enhances Phase 1 (polish on LLM integration)

**Implementation Tasks:**
1. Upgrade event loop to select()-based polling
2. Enable terminal input during LLM streaming
3. Add cancellation support (ESC or Ctrl+C during stream)
4. Enable scrollback navigation during streaming (Page Up/Down)
5. Improve error recovery and partial response handling

**Technical Details:**
- Upgrade from blocking (Option A) to select() polling (Option B)
- Monitor both STDIN and HTTP socket
- User can scroll, type, or cancel during response
- Still single-threaded

**Deliverable:** Non-blocking UI, can scroll/cancel during AI responses.

**Testing:**
- Test input handling during streaming
- Test cancellation mid-stream
- Test scrollback navigation during stream
- Test error handling (connection drop during stream)

---

## Phase 5: Tool Execution

**Dependencies:** Depends on Phase 1 (LLM) + Phase 3 (Database)

**Implementation Tasks:**
1. Tool interface design (follows provider pattern)
2. Implement database tools (search_messages, load_context)
3. Implement file operations (read, write, search)
4. Implement shell command execution
5. Parse tool calls from LLM responses (OpenAI format initially)
6. Execute tools and inject results into conversation
7. Tree-sitter integration for code analysis

**Technical Details:**
- Tool interface: `ik_tool_t` with execute function pointer
- LLM can search conversation history via database tools
- File operations: read file, write file, search files
- Shell: execute commands, capture output
- Tool results injected as messages in conversation

**Deliverable:** AI can search conversation history, execute commands, read/write files, analyze code.

**Testing:**
- Unit test each tool implementation
- Mock tool execution in tests
- Test tool result injection into conversation
- Integration: LLM calls tool → tool executes → result shown

---

## Phase 6: Multi-LLM Provider Support

**Dependencies:** Depends on Phase 1 (HTTP client)

**Implementation Tasks:**
1. Abstract provider interface (ik_provider_t)
2. Refactor OpenAI client to use interface
3. Add tool format translation per provider
4. Implement Anthropic provider
5. Implement Google provider (Gemini)
6. Implement X.AI provider (Grok)
7. Add `/model <name>` command
8. Update config for multiple API keys

**Technical Details:**
- Provider interface: init, send_streaming, format_messages, format_tools
- Each provider translates to its own API format
- Model switching via `/model` command
- Config has API keys for all providers

**Deliverable:** Support for multiple AI providers with tool calling, runtime switching.

**Testing:**
- Unit test each provider implementation
- Mock responses for each provider
- Test provider switching
- Test tool format translation
- Integration: same conversation works across providers

---

## Summary: Dependency Graph

```
Phase 1: LLM HTTP Client (no dependencies)
    ↓ required by
Phase 3: Database Integration
    ↓ enhances
Phase 4: Interactive Streaming
    ↓ enables
Phase 5: Tool Execution

Phase 2: Command Infrastructure (no dependencies, can run parallel)
    ↓ enhances all phases

Phase 6: Multi-LLM Support (depends on Phase 1 only)
```

**Recommended Order:**
1. Phase 1 + Phase 2 (parallel) - Get basic functionality working
2. Phase 3 - Add persistence
3. Phase 4 - Polish UX
4. Phase 5 - Add tool execution
5. Phase 6 - Add multi-provider support

---

## Milestones

### Milestone 1: Working Chat (Phase 1 + Phase 2)
- User can chat with OpenAI GPT models
- Streaming responses displayed in real-time
- Basic commands: /help, /quit, /clear, /mark, /rewind
- No persistence (messages lost on exit)

**Timeline:** ~2-3 weeks

### Milestone 2: Persistent Chat (Phase 3)
- All messages saved to PostgreSQL
- Session tracking
- Full-text search available
- Mark/rewind persisted to database

**Timeline:** ~1-2 weeks

### Milestone 3: Polished UX (Phase 4)
- Interactive during streaming
- Can cancel/scroll during response
- Better error handling

**Timeline:** ~1 week

### Milestone 4: Tool-Enabled Agent (Phase 5)
- LLM can search conversation history
- LLM can read/write files
- LLM can execute shell commands
- LLM can analyze code

**Timeline:** ~3-4 weeks

### Milestone 5: Multi-Provider Support (Phase 6)
- Support for Anthropic, Google, X.AI
- Runtime provider switching
- Unified tool interface

**Timeline:** ~2-3 weeks

**Total estimated timeline for v1.0:** ~9-13 weeks

---

## v1.0 Completion Criteria

**Must Have:**
- ✅ Streaming LLM responses (OpenAI)
- ✅ PostgreSQL persistence
- ✅ Session tracking
- ✅ /clear, /mark, /rewind commands
- ✅ Interactive streaming (non-blocking)
- ✅ Database search tools
- ✅ File operation tools
- ✅ Shell execution tools

**Should Have:**
- ✅ Multi-provider support (Anthropic, Google, X.AI)
- ✅ Code analysis (tree-sitter)
- ✅ Syntax highlighting in scrollback

**Nice to Have (defer to v2.0):**
- External editor integration ($EDITOR)
- Command history persistence
- Rich formatting and themes
- Automatic context summarization
- RAG-based context retrieval

---

## Risk Mitigation

### Risk: libcurl Complexity

**Mitigation:**
- Start with blocking approach (simpler)
- Upgrade to select() polling in Phase 4
- Extensive testing with mock responses

### Risk: Database Performance

**Mitigation:**
- Profile INSERT performance early
- Use indexes appropriately
- Consider batching if needed (but start with immediate writes)

### Risk: Multi-Provider API Differences

**Mitigation:**
- Design provider interface carefully in Phase 6
- Start with OpenAI (well-documented)
- Add providers incrementally
- Test tool calling for each provider separately

### Risk: Tool Security

**Mitigation:**
- Clear documentation: full trust model
- No sandboxing in v1.0
- User responsible for tool safety
- Consider sandboxing in v2.0

---

## Related Documentation

- **[v1-architecture.md](v1-architecture.md)** - Overall v1.0 architecture
- **[v1-llm-integration.md](v1-llm-integration.md)** - HTTP client and streaming design
- **[v1-database-design.md](v1-database-design.md)** - Database schema and persistence
- **[v1-conversation-management.md](v1-conversation-management.md)** - Message lifecycle and commands
