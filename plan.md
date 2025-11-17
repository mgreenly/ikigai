# OpenAI API Client Implementation Plan

## Overview

Integrate OpenAI API client with streaming support into the existing REPL, enabling basic conversation flow where user messages are sent to the LLM and responses stream into the scrollback buffer.

**Architecture Reference:** See `docs/logical-architecture-analysis.md` for complete architectural analysis including:
- Three-layer architecture (scrollback → session messages → database)
- Scrollback as context window principle
- Message lifecycle and ownership
- Future phases (database, tools, multi-LLM)

**This document focuses on Phase 1: HTTP LLM Integration only.**

## User Experience Flow

1. **User types question** → presses Enter
2. **Question moves to scrollback** (displayed as user's message)
3. **Input area replaced with spinner** (everything below `───────...` separator)
   - Spinner animates at 80ms intervals
   - User cannot type (blocked state)
4. **API request happens** (concurrent with spinner animation)
5. **Response streams in** (if API supports streaming):
   - Tokens append to scrollback immediately as they arrive
   - Viewport auto-scrolls to show new content
6. **Response completes** → spinner removed → input area returns
7. **User can type next question**

### Streaming Strategy

- **Streaming APIs** (e.g., OpenAI) - tokens arrive incrementally, append each to scrollback in real-time
- **Non-streaming APIs** - single complete response, append all at once
- Design abstraction to handle both modes transparently

### Error Handling (v1)

- HTTP/API errors → format error message and response body as text
- Append to scrollback (user sees what went wrong)
- Return to input mode (user can retry or adjust)

## Architectural Decisions

### 1. Threading/Async Model ✅ RESOLVED
- User is blocked (cannot type) during API requests
- Terminal event loop continues running for spinner animation
- HTTP operations use non-blocking I/O integrated into `select()` event loop
- REPL state machine: `IDLE` (normal input) ↔ `WAITING_FOR_LLM` (spinner showing, input blocked)
- Timer events drive spinner animation (80ms intervals)
- HTTP socket fd added to event loop's `select()` call

### 2. Layer-Based Rendering Architecture ✅ RESOLVED

**Core Concept: Layer Cake Model**

The UI is a single continuous scrollable document (the "layer cake") with the viewport as a window into it:

```
┌─────────────────────┐  ← Top of layer cake
│ Scrollback buffer   │
│   (messages, etc)   │
├─────────────────────┤
│ Spinner             │  ← Visible only when WAITING_FOR_LLM
├─────────────────────┤
│ Separator (──────)  │
├─────────────────────┤
│ Input zone          │  ← Hidden when WAITING_FOR_LLM
└─────────────────────┘  ← Bottom of layer cake

       ↕ Viewport scrolls through entire cake
```

**Key Principles:**

1. **Everything scrolls** - no "fixed" vs "scrollable" layers
2. **Viewport is a window** - shows a portion of the total cake based on scroll position
3. **All layers exist** - but may be hidden (0 height, don't render)
4. **Layers always in same order** - fixed stacking, visibility changes

**Layer Visibility:**
- Visible layers contribute height to the cake
- Hidden layers contribute 0 height (skipped during rendering)
- Example: `WAITING_FOR_LLM` state → spinner visible, input hidden

**Auto-scroll Behaviors:**
- New content in scrollback → viewport stays put (top line remains on same buffer line)
- User starts typing → snap viewport to show cursor as bottom line, BUT only if:
  - The input zone is currently scrolled out of view
  - The spinner is NOT visible (not in WAITING_FOR_LLM state)
- Input already visible → no snap needed
- While spinner is showing → discard all typed characters, no viewport changes
- Page Up/Down → manual scroll through the entire cake

**Initial Implementation (v1):**

Four layers only:
1. **Scrollback buffer** - existing, wrapped in layer abstraction
2. **Spinner** - new layer, visible during LLM requests
3. **Separator** - existing, wrapped in layer abstraction
4. **Input zone** - existing, wrapped in layer abstraction

**Future Layers (not implemented yet):**
- Tool feedback (between buffer and separator)
- Token usage display (near separator)
- Status bar (below input)

## Answered Questions

### HTTP & API Integration

#### 1. HTTP Client Library ✅

**Decision: libcurl**

- Industry standard, mature, battle-tested
- Excellent async support via `curl_multi_*` interface
- `curl_multi_fdset()` integrates perfectly with `select()` event loop
- Built-in TLS/SSL support via OpenSSL/GnuTLS backends
- Acceptable dependency (ubiquitous on Linux)
- ulfius is HTTP **server** only, not suitable for client

**Integration pattern:**
```c
// In event loop:
curl_multi_fdset(multi_handle, &read_fds, &write_fds, &exc_fds, &max_fd);
select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);
curl_multi_perform(multi_handle, &running_handles);
```

#### 2. Streaming Integration ✅

**OpenAI SSE Format:**
```
data: {"choices":[{"delta":{"content":"Hello"}}]}

data: {"choices":[{"delta":{"content":" world"}}]}

data: [DONE]
```

**Parsing Strategy:**

1. **Buffered line reader** - accumulate bytes until `\n\n` delimiter
2. **Extract JSON** - strip `data: ` prefix
3. **Parse incrementally** - yyjson parses each complete chunk
4. **Extract content delta** - path: `choices[0].delta.content`
5. **Append to scrollback** - token by token

**Error recovery:**
- Incomplete chunk → keep in buffer, wait for more data
- Malformed JSON → log error, skip chunk, continue streaming
- Connection drop → display partial response, mark as incomplete

#### 3. REPL Message Handling ✅

**Simple prefix-based approach:**

- **`/` prefix** → REPL command (e.g., `/clear`, `/help`, `/model gpt-4`)
- **Everything else** → send to LLM

**Commands to implement (Phase 1):**
- `/clear` - clear scrollback, session messages, and marks
- `/help` - show available commands
- `/mark [label]` - create checkpoint for rollback (see logical-architecture-analysis.md)
- `/rewind [label]` - rollback to checkpoint (see logical-architecture-analysis.md)
- `/model <name>` - switch model (gpt-4-turbo, gpt-3.5-turbo, etc.)
- `/system <text>` - set system message

**Commands deferred to later phases:**
- Database operations will be tool-based when implemented, not user commands

**Rationale:** Explicit send not needed - pressing Enter always sends. Simple and intuitive.

#### 4. Multi-turn Conversations ✅

**Data structure (Three-Layer Architecture):**
```c
// Message structure (stored in session_messages[], sent to API, persisted to DB later)
typedef struct {
    int64_t id;                    // Database primary key (0 if not persisted yet)
    char *role;                    // "user", "assistant", "system", "mark", "rewind"
    char *content;                 // Text content or label for marks
    char *timestamp;               // ISO 8601
    int32_t tokens;                // Token count (if available)
    char *model;                   // Model identifier
    int64_t rewind_to_message_id;  // For rewind messages: points to mark
} ik_message_t;

// Mark structure (for /mark and /rewind commands)
typedef struct {
    size_t message_index;          // Position in session_messages[] array
    int64_t db_message_id;         // ID of the mark message (0 if not persisted)
    char *label;                   // Optional user label (or NULL)
    char *timestamp;               // ISO 8601
} ik_mark_t;

// REPL context extended with session messages
struct ik_repl_ctx_t {
    // Existing fields...
    ik_term_ctx_t *term;
    ik_render_ctx_t *render;
    ik_input_buffer_t *input_buffer;
    ik_scrollback_t *scrollback;

    // NEW: Session messages (active LLM context)
    ik_message_t **session_messages;   // What LLM receives
    size_t session_message_count;

    // NEW: Checkpoint management
    ik_mark_t **marks;                 // Stack of marks (LIFO)
    size_t mark_count;

    // NEW: Streaming state
    char *streaming_buffer;            // Accumulates current LLM response
    size_t streaming_buffer_len;
    bool is_streaming;

    // NEW: LLM client
    ik_llm_client_t *llm;
};
```

**API request format:**
```json
{
  "model": "gpt-4-turbo",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant."},
    {"role": "user", "content": "Hello!"},
    {"role": "assistant", "content": "Hi! How can I help?"},
    {"role": "user", "content": "What's the weather?"}
  ],
  "stream": true
}
```

**Include all messages** in each request to maintain context.

**Key Architectural Points:**
- **Session messages (`session_messages[]`)** are the source of truth for LLM context
- **Scrollback** is the rendered view of session messages (with decorations)
- Messages are **NOT** persisted to database in this phase (memory only)
- `/clear` clears both scrollback and session messages
- `/mark` and `/rewind` provide checkpoint/rollback within session
- Database persistence is a future phase (see logical-architecture-analysis.md)

### State Management

#### 5. Conversation State ✅

**Storage:**
- **In-memory only** - `session_messages[]` array in `ik_repl_ctx_t`
- **Lifetime**: Entire app session (cleared by `/clear` command)
- **Messages lost on exit** - acceptable for Phase 1
- **Database integration**: Later phase (see logical-architecture-analysis.md)

**Token limit handling (v1):**
- **No truncation initially** - assume reasonable conversation lengths
- **Future enhancement**: Count tokens, truncate oldest messages when approaching limit
- **OpenAI limits**: gpt-4-turbo (128k), gpt-3.5-turbo (16k)

#### 6. Configuration ✅

**Use existing config module** (`src/config.h`)

**Config fields to add to `ik_cfg_t`:**
```c
typedef struct {
    char *openai_api_key;        // Existing
    char *openai_model;           // NEW: "gpt-4-turbo", "gpt-3.5-turbo", etc.
    double openai_temperature;    // NEW: 0.0-2.0, default 0.7
    int32_t openai_max_tokens;    // NEW: default 4096
    char *openai_system_message;  // NEW: optional, can be NULL

    // Legacy fields (will be removed after server/protocol cleanup)
    char *listen_address;
    uint16_t listen_port;
} ik_cfg_t;
```

**Default config (in `create_default_config`):**
```json
{
  "openai_api_key": "YOUR_API_KEY_HERE",
  "openai_model": "gpt-5-mini",
  "openai_temperature": 0.7,
  "openai_max_tokens": 4096,
  "openai_system_message": null
}
```

**Runtime access:**
- REPL loads config once at startup: `cfg = TRY(ik_cfg_load(ctx, "~/.config/ikigai/config.json"));`
- Pass `cfg` to OpenAI client functions
- `/model` command modifies in-memory config (not persisted)

### Development Strategy

#### 7. Incremental Implementation Approach ✅

**Recommended order (TDD-compliant - see Implementation Phases below):**

**Phase 1.1: Configuration Extension**
- Extend config module with OpenAI parameters
- Small, isolated changes with tests

**Phase 1.2-1.4: Layer Abstraction & Rendering**
- Design layer API (`ik_layer_t` interface)
- Refactor existing rendering to use layers (scrollback, separator, input)
- Add spinner layer
- Test layer visibility toggling

**Phase 1.5: HTTP Client Module**
- Create `src/openai/` module
- Implement simple non-streaming request (test with fixtures)
- Add streaming support (parse SSE format)
- Unit tests with fixture files

**Phase 1.6-1.7: Integration & Commands**
- Wire HTTP client into REPL event loop
- Add `WAITING_FOR_LLM` state
- Implement command registry and essential commands (/clear, /mark, /rewind)
- Connect Enter key → API request → scrollback append
- Test end-to-end flow

**Phase 1.8: Verification & Polish**
- Mock verification against real API
- Final manual testing
- Documentation updates

**Rationale:** Layer abstraction early because it requires refactoring existing code (riskier). HTTP client is isolated and can be tested independently.

#### 8. Testing Approach ✅

**Two-tier testing strategy:**

**Tier 1: Fixture-based unit tests** (always run)
- Mock HTTP responses using fixture files
- Fast, deterministic, no network dependencies
- Example fixtures in `tests/fixtures/openai/`:
  - `stream_hello_world.txt` - Simple streaming response
  - `stream_multiline.txt` - Multi-line code response
  - `stream_done.txt` - Complete conversation with [DONE] marker
  - `error_401_unauthorized.json` - Invalid API key
  - `error_429_rate_limit.json` - Rate limit exceeded
  - `error_500_server.json` - Server error

**Tier 2: Verification tests** (manual, with real API)
```bash
# Only run when explicitly requested (requires valid API key)
OPENAI_API_KEY=sk-... make verify-mocks
```

**Verification test structure:**
```c
// In tests/integration/openai_mock_verification_test.c
START_TEST(verify_stream_response_fixture) {
    // Skip if VERIFY_MOCKS not set
    if (!getenv("VERIFY_MOCKS")) {
        ck_assert(true);  // Pass without running
        return;
    }

    // Make real API call
    res_t real_response = ik_openai_chat_create(ctx, cfg, "Hello!");

    // Load fixture
    char *fixture = load_fixture("tests/fixtures/openai/stream_hello_world.txt");

    // Compare structure (not exact content, since LLM responses vary)
    ck_assert(responses_have_same_structure(real_response, fixture));
    ck_assert(both_have_required_fields(real_response, fixture));
}
```

**Mock creation workflow:**
1. Write test that needs a fixture
2. Make real API call with `VERIFY_MOCKS=1`
3. Save response to fixture file
4. Review fixture for sensitive data (scrub if needed)
5. Commit fixture to repo
6. Regular tests use fixture

**Layer abstraction testing:**
- Unit tests: Each layer's visibility, height calculation, rendering independently
- Integration tests: Layer cake rendering with different state combinations
- Viewport tests: Scrolling through multi-layer document

**Spinner testing:**
- Static tests: Render each frame, verify output
- Animation tests: Timer callbacks update state correctly
- No real-time tests: Test frame transitions, not timing

**OOM injection:**
- All allocation paths covered (libcurl setup, JSON parsing, message storage)
- 100% coverage maintained throughout

#### 9. Layer Abstraction API Design ✅

**Core interface:**
```c
typedef struct ik_layer ik_layer_t;

typedef bool (*ik_layer_is_visible_fn)(const ik_layer_t *layer);
typedef size_t (*ik_layer_get_height_fn)(const ik_layer_t *layer, size_t width);
typedef ik_result_t (*ik_layer_render_fn)(const ik_layer_t *layer, ik_output_t *output,
                                           size_t width, size_t start_row, size_t row_count);

struct ik_layer {
    const char *name;                      // "scrollback", "spinner", "separator", "input"
    void *data;                            // Layer-specific context (e.g., ik_scrollback_t *)
    ik_layer_is_visible_fn is_visible;
    ik_layer_get_height_fn get_height;
    ik_layer_render_fn render;
};
```

**Layer cake manager:**
```c
typedef struct {
    ik_layer_t **layers;    // Ordered array (top to bottom)
    size_t layer_count;
    size_t viewport_row;    // Current scroll position
    size_t viewport_height; // Terminal height
} ik_layer_cake_t;

// Calculate total visible height
size_t ik_layer_cake_get_total_height(const ik_layer_cake_t *cake, size_t width);

// Render visible portion of cake
ik_result_t ik_layer_cake_render(const ik_layer_cake_t *cake, ik_output_t *output, size_t width);
```

**Ownership:**
- **REPL owns**: `ik_layer_cake_t` instance
- **Layers reference**: Existing data (scrollback, input buffer) - no duplication
- **Lifecycle**: Layers created on REPL init, destroyed on shutdown

**Data access:**
- Each layer's `data` pointer holds layer-specific context
- Example: Scrollback layer's `data` → `ik_scrollback_t *`
- Example: Spinner layer's `data` → `ik_spinner_state_t *` (frame index, visible flag)

## Implementation Tasks

**See `tasks.md` for detailed implementation tasks organized by phase.**

The implementation is divided into 8 sub-phases:
- **Phase 1.1:** Configuration Extension
- **Phase 1.2:** Layer Abstraction Foundation
- **Phase 1.3:** Refactor Existing Rendering to Layers
- **Phase 1.4:** Spinner Layer
- **Phase 1.5:** HTTP Client Module (libcurl)
- **Phase 1.6:** Event Loop Integration (Non-blocking I/O)
- **Phase 1.7:** Command Infrastructure & Manual Testing
- **Phase 1.8:** Mock Verification & Polish

## Success Criteria (Phase 1)

Phase 1 implementation is complete when:

1. ✅ All unit tests pass (100% coverage)
2. ✅ All integration tests pass
3. ✅ All quality gates pass (fmt, lint, check-dynamic, valgrind)
4. ✅ All manual test sessions completed successfully
5. ✅ Mock verification tests pass against real API
6. ✅ Documentation updated
7. ✅ Final acceptance test passed by human
8. ✅ Basic LLM chat works with streaming responses
9. ✅ Commands work: /clear, /mark, /rewind, /help, /model, /system
10. ✅ Messages stored in-memory only (database is Phase 2)

## Next Steps After Phase 1

See `docs/logical-architecture-analysis.md` for:
- Phase 2: Database Integration (sessions, persistence)
- Phase 3: Tool Execution (search, file ops, shell commands)
- Phase 4: Multi-LLM Support (Anthropic, Google, X.AI)
