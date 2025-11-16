# OpenAI API Client Implementation Plan

## Overview

Integrate OpenAI API client with streaming support into the existing REPL, enabling basic conversation flow where user messages are sent to the LLM and responses stream into the scrollback buffer.

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
- New content in scrollback → viewport scrolls to show bottom
- User starts typing (input scrolled out of view) → snap viewport to show input at bottom
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

**Commands to implement (v1):**
- `/clear` - clear scrollback and conversation history
- `/help` - show available commands
- `/model <name>` - switch model (gpt-4-turbo, gpt-3.5-turbo, etc.)
- `/system <text>` - set system message

**Rationale:** Explicit send not needed - pressing Enter always sends. Simple and intuitive.

#### 4. Multi-turn Conversations ✅

**Data structure:**
```c
typedef struct {
    char *role;     // "user", "assistant", "system"
    char *content;  // message text
} ik_openai_msg_t;

typedef struct {
    ik_openai_msg_t **messages;  // array of message pointers
    size_t count;
    char *system_message;        // optional system prompt
} ik_openai_conversation_t;
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

### State Management

#### 5. Conversation State ✅

**Storage:**
- **In-memory for v1** - simple array/list in REPL context
- **Structure**: `ik_openai_conversation_t` (see above)
- **Lifetime**: Entire session (cleared by `/clear` command)
- **Future**: PostgreSQL integration (later phase)

**Token limit handling (v1):**
- **No truncation initially** - assume reasonable conversation lengths
- **Future enhancement**: Count tokens (tiktoken library), truncate oldest messages when approaching limit
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
  "openai_model": "gpt-4-turbo",
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

**Recommended order (TDD-compliant):**

**Phase 1: Configuration Extension**
- Extend config module with OpenAI parameters
- Small, isolated changes with tests

**Phase 2: Layer Abstraction**
- Design layer API (`ik_layer_t` interface)
- Refactor existing rendering to use layers (scrollback, separator, input)
- Add spinner layer
- Test layer visibility toggling

**Phase 3: HTTP Client Module**
- Create `src/openai/` module
- Implement simple non-streaming request (test with fixtures)
- Add streaming support (parse SSE format)
- Unit tests with fixture files

**Phase 4: Integration**
- Wire HTTP client into REPL event loop
- Add `WAITING_FOR_LLM` state
- Connect Enter key → API request → scrollback append
- Test end-to-end flow

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

## Implementation Phases

### Phase 1: Configuration Extension

**Goal:** Add OpenAI configuration fields to existing config module

#### Task 1.1: Extend ik_cfg_t structure
- [ ] Add `openai_model` field (char *)
- [ ] Add `openai_temperature` field (double)
- [ ] Add `openai_max_tokens` field (int32_t)
- [ ] Add `openai_system_message` field (char *, nullable)
- [ ] Update header file `src/config.h`
- **Tests:** Compile-only (structure change)

#### Task 1.2: Update default config creation
- [ ] Add default values to `create_default_config()` JSON
- [ ] Set `openai_model` = "gpt-4-turbo"
- [ ] Set `openai_temperature` = 0.7
- [ ] Set `openai_max_tokens` = 4096
- [ ] Set `openai_system_message` = null
- **Tests:** Integration test verifies default config file contains new fields
- **Manual verification:** Delete `~/.config/ikigai/config.json`, run `make test`, verify defaults

#### Task 1.3: Add parsing for new fields
- [ ] Parse `openai_model` field (required, string)
- [ ] Parse `openai_temperature` field (required, number, range 0.0-2.0)
- [ ] Parse `openai_max_tokens` field (required, integer, range 1-128000)
- [ ] Parse `openai_system_message` field (optional, string or null)
- [ ] Add validation for ranges
- **Tests:** Unit tests for valid/invalid values, missing fields, type errors
- **Coverage:** OOM injection on all allocation paths

#### Task 1.4: Update integration tests
- [ ] Test loading config with all new fields
- [ ] Test validation errors (temperature out of range, etc.)
- [ ] Test optional system_message (null and string values)
- **Tests:** `make check` passes with 100% coverage
- **Manual verification:** Review test output

#### Task 1.5: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0% on all metrics
- [ ] Run `make check-dynamic` - all sanitizers pass
- **Manual verification:** Review all quality gate outputs

### Phase 2: Layer Abstraction Foundation

**Goal:** Create layer abstraction API and refactor existing rendering

#### Task 2.1: Design layer interface
- [ ] Create `src/layer.h` with `ik_layer_t` structure
- [ ] Define `ik_layer_is_visible_fn` function pointer type
- [ ] Define `ik_layer_get_height_fn` function pointer type
- [ ] Define `ik_layer_render_fn` function pointer type
- [ ] Document layer contract in comments
- **Tests:** Header-only, compile verification

#### Task 2.2: Create layer constructor/destructor
- [ ] Write `ik_layer_create()` function
- [ ] Write `ik_layer_destroy()` function (talloc-based)
- [ ] Add precondition assertions
- **Tests:** Unit test - create/destroy layer, verify talloc hierarchy
- **Coverage:** OOM injection

#### Task 2.3: Create layer cake manager structure
- [ ] Create `src/layer_cake.h` with `ik_layer_cake_t` structure
- [ ] Add `layers` array field
- [ ] Add `layer_count`, `viewport_row`, `viewport_height` fields
- [ ] Write `ik_layer_cake_create()` function
- [ ] Write `ik_layer_cake_destroy()` function
- **Tests:** Unit test - create/destroy cake
- **Coverage:** OOM injection

#### Task 2.4: Implement ik_layer_cake_add_layer()
- [ ] Write function to append layer to cake
- [ ] Resize layers array as needed
- [ ] Maintain layer ordering (top to bottom)
- **Tests:** Unit test - add 1, 2, 4 layers, verify order
- **Coverage:** OOM injection on array resize

#### Task 2.5: Implement ik_layer_cake_get_total_height()
- [ ] Iterate through all layers
- [ ] Call `is_visible()` for each layer
- [ ] Sum `get_height()` for visible layers only
- [ ] Return total height
- **Tests:** Unit test with mock layers (different visibility combinations)

#### Task 2.6: Implement ik_layer_cake_render()
- [ ] Calculate which layers are in viewport range
- [ ] Call `render()` on each visible layer in viewport
- [ ] Handle partial layer rendering (viewport cuts through layer)
- [ ] Accumulate output
- **Tests:** Unit test with mock layers, verify correct layers rendered
- **Coverage:** Edge cases (viewport at top, bottom, middle)

#### Task 2.7: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review outputs

### Phase 3: Refactor Existing Rendering to Layers

**Goal:** Wrap existing scrollback, separator, input in layer abstraction

#### Task 3.1: Create scrollback layer wrapper
- [ ] Write `ik_scrollback_layer_create()` function
- [ ] Implement `scrollback_is_visible()` (always true)
- [ ] Implement `scrollback_get_height()` (delegate to existing scrollback)
- [ ] Implement `scrollback_render()` (delegate to existing render function)
- [ ] Store `ik_scrollback_t*` in layer's `data` pointer
- **Tests:** Unit test - create scrollback layer, verify delegation
- **Coverage:** OOM injection

#### Task 3.2: Create separator layer wrapper
- [ ] Write `ik_separator_layer_create()` function
- [ ] Implement `separator_is_visible()` (always true)
- [ ] Implement `separator_get_height()` (always 1)
- [ ] Implement `separator_render()` (render separator line)
- **Tests:** Unit test - render separator at various widths
- **Coverage:** OOM injection

#### Task 3.3: Create input layer wrapper
- [ ] Write `ik_input_layer_create()` function
- [ ] Implement `input_is_visible()` (check REPL state)
- [ ] Implement `input_get_height()` (delegate to existing input buffer)
- [ ] Implement `input_render()` (delegate to existing render function)
- [ ] Store `ik_input_buf_t*` in layer's `data` pointer
- **Tests:** Unit test - visibility changes based on state
- **Coverage:** OOM injection, state variations

#### Task 3.4: Integrate layers into REPL
- [ ] Create `ik_layer_cake_t` in REPL context
- [ ] Add scrollback layer to cake
- [ ] Add separator layer to cake
- [ ] Add input layer to cake
- [ ] Verify layer ordering
- **Tests:** Integration test - REPL creates cake with 3 layers
- **Coverage:** OOM during REPL initialization

#### Task 3.5: Replace REPL render with layer_cake_render
- [ ] Remove old direct rendering code
- [ ] Call `ik_layer_cake_render()` instead
- [ ] Verify output identical to before
- **Tests:** Regression tests - output matches old implementation
- **Manual verification:** Run `bin/ikigai`, verify UI looks identical

#### Task 3.6: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Interactive REPL testing (multi-line input, scrolling, etc.)

### Phase 4: Spinner Layer

**Goal:** Add animated spinner layer for LLM wait state

#### Task 4.1: Create spinner state structure
- [ ] Define `ik_spinner_state_t` structure
- [ ] Add `frame_index` field (which animation frame)
- [ ] Add `visible` flag
- [ ] Write `ik_spinner_state_create()` function
- **Tests:** Unit test - create spinner state
- **Coverage:** OOM injection

#### Task 4.2: Implement spinner animation frames
- [ ] Define spinner frames: `|`, `/`, `-`, `\`
- [ ] Write `ik_spinner_get_frame()` function (returns current char)
- [ ] Write `ik_spinner_advance()` function (cycles frame_index)
- **Tests:** Unit test - cycle through all 4 frames
- **Coverage:** Full frame cycle

#### Task 4.3: Create spinner layer wrapper
- [ ] Write `ik_spinner_layer_create()` function
- [ ] Implement `spinner_is_visible()` (check spinner state)
- [ ] Implement `spinner_get_height()` (1 if visible, 0 if hidden)
- [ ] Implement `spinner_render()` (render current frame with message)
- [ ] Store `ik_spinner_state_t*` in layer's `data` pointer
- **Tests:** Unit test - visibility, height, rendering
- **Coverage:** OOM injection

#### Task 4.4: Add spinner to layer cake
- [ ] Insert spinner layer between scrollback and separator
- [ ] Verify layer ordering: scrollback, spinner, separator, input
- **Tests:** Integration test - layer cake has 4 layers in correct order
- **Manual verification:** Inspect layer cake structure

#### Task 4.5: Add REPL state for spinner control
- [ ] Add `WAITING_FOR_LLM` state to REPL state enum
- [ ] Add spinner visibility toggle function
- [ ] Add spinner frame advance function (called on timer)
- **Tests:** Unit test - state transitions, visibility changes
- **Coverage:** All state transitions

#### Task 4.6: Add timer event for spinner animation
- [ ] Add 80ms timer to event loop (only when WAITING_FOR_LLM)
- [ ] Timer callback advances spinner frame
- [ ] Trigger re-render after each frame advance
- **Tests:** Unit test - timer triggers, frame advances, render called
- **Manual verification:** Can be tested manually in Phase 6

#### Task 4.7: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review test outputs

### Phase 5: HTTP Client Module (libcurl)

**Goal:** Create OpenAI HTTP client with streaming support

#### Task 5.1: Create module structure
- [ ] Create `src/openai/` directory
- [ ] Create `src/openai/client.h` header
- [ ] Create `src/openai/client.c` implementation
- [ ] Add to build system (Makefile)
- **Tests:** Compile-only verification

#### Task 5.2: Define OpenAI message structures
- [ ] Define `ik_openai_msg_t` structure (role, content)
- [ ] Define `ik_openai_conversation_t` structure (messages array)
- [ ] Write `ik_openai_msg_create()` function
- [ ] Write `ik_openai_conversation_create()` function
- [ ] Write `ik_openai_conversation_add_message()` function
- **Tests:** Unit test - create messages, build conversation
- **Coverage:** OOM injection on all allocations

#### Task 5.3: Define request/response structures
- [ ] Define `ik_openai_request_t` structure (model, messages, temperature, etc.)
- [ ] Define `ik_openai_response_t` structure (content, finish_reason, usage)
- [ ] Write constructor/destructor functions
- **Tests:** Unit test - create/destroy structures
- **Coverage:** OOM injection

#### Task 5.4: Implement JSON request serialization
- [ ] Write `ik_openai_serialize_request()` function
- [ ] Use yyjson to build JSON request body
- [ ] Include model, messages array, temperature, max_tokens, stream=true
- [ ] Format according to OpenAI API spec
- **Tests:** Unit test - serialize request, verify JSON structure
- **Coverage:** OOM injection in yyjson allocations

#### Task 5.5: Create test fixtures
- [ ] Create `tests/fixtures/openai/stream_hello_world.txt`
- [ ] Create `tests/fixtures/openai/stream_multiline.txt`
- [ ] Create `tests/fixtures/openai/stream_done.txt`
- [ ] Create `tests/fixtures/openai/error_401_unauthorized.json`
- [ ] Create `tests/fixtures/openai/error_429_rate_limit.json`
- [ ] Create `tests/fixtures/openai/error_500_server.json`
- **Manual step:** Make real API calls to capture responses
- **Manual verification:** Review fixtures for sensitive data, scrub if needed

#### Task 5.6: Implement SSE parser (buffered line reader)
- [ ] Write `ik_openai_sse_parser_create()` function
- [ ] Implement `ik_openai_sse_parser_feed()` - accumulate bytes
- [ ] Detect `\n\n` delimiter (complete SSE event)
- [ ] Extract complete events, keep incomplete data in buffer
- **Tests:** Unit test - feed partial data, verify buffering
- **Coverage:** Various buffer sizes, edge cases

#### Task 5.7: Implement SSE event parsing
- [ ] Write `ik_openai_parse_sse_event()` function
- [ ] Strip `data: ` prefix
- [ ] Handle `data: [DONE]` marker (end of stream)
- [ ] Parse JSON chunk using yyjson
- [ ] Extract `choices[0].delta.content` field
- [ ] Return content string or NULL if [DONE]
- **Tests:** Unit test - parse fixture events, verify content extraction
- **Coverage:** Malformed events, missing fields, [DONE] marker

#### Task 5.8: Implement libcurl HTTP client (synchronous first)
- [ ] Write `ik_openai_http_post()` function
- [ ] Initialize libcurl easy handle
- [ ] Set URL, headers (Authorization, Content-Type)
- [ ] Set request body
- [ ] Set write callback for response data
- [ ] Execute synchronous request
- [ ] Return response body
- **Tests:** Unit test with mock server or fixtures
- **Coverage:** OOM injection, libcurl errors

#### Task 5.9: Add streaming support to HTTP client
- [ ] Modify write callback to feed SSE parser incrementally
- [ ] Call callback for each extracted content chunk
- [ ] Handle partial responses
- **Tests:** Unit test - feed fixture data in chunks, verify callbacks
- **Coverage:** Various chunk sizes, connection errors

#### Task 5.10: Implement ik_openai_chat_create() (high-level API)
- [ ] Write main API function
- [ ] Takes conversation, config, callback function
- [ ] Serializes request
- [ ] Makes HTTP call with streaming
- [ ] Invokes callback for each content chunk
- [ ] Returns complete response
- **Tests:** Unit test with fixtures - verify end-to-end flow
- **Coverage:** All error paths (HTTP errors, parse errors, etc.)

#### Task 5.11: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review all test outputs

### Phase 6: Event Loop Integration (Non-blocking I/O)

**Goal:** Integrate libcurl multi interface with REPL event loop

#### Task 6.1: Refactor HTTP client to use curl_multi
- [ ] Replace `curl_easy_perform()` with `curl_multi_*` API
- [ ] Write `ik_openai_client_multi_create()` function
- [ ] Write `ik_openai_client_multi_add_request()` function
- [ ] Write `ik_openai_client_multi_perform()` function
- [ ] Return file descriptors for select() integration
- **Tests:** Unit test - verify non-blocking behavior
- **Coverage:** curl_multi errors, handle management

#### Task 6.2: Add curl FDs to REPL select() call
- [ ] Call `curl_multi_fdset()` to get read/write/except fd sets
- [ ] Merge with existing terminal fd in select()
- [ ] Calculate correct timeout (min of timer timeout and curl timeout)
- **Tests:** Integration test - mock curl FDs, verify select() behavior
- **Coverage:** FD set handling, timeout calculation

#### Task 6.3: Add curl_multi_perform() to event loop
- [ ] Check if curl FDs are ready after select()
- [ ] Call `curl_multi_perform()` if ready
- [ ] Process completed transfers
- [ ] Invoke callbacks for streaming chunks
- **Tests:** Integration test - simulate ready FDs, verify perform called
- **Coverage:** Transfer completion, errors

#### Task 6.4: Add REPL state machine transitions
- [ ] Add `IDLE` state (normal input)
- [ ] Add `WAITING_FOR_LLM` state (spinner visible, input hidden)
- [ ] Implement state transition on Enter key (IDLE → WAITING_FOR_LLM)
- [ ] Implement state transition on response complete (WAITING_FOR_LLM → IDLE)
- **Tests:** Unit test - verify state transitions
- **Coverage:** All transition paths

#### Task 6.5: Wire Enter key to API request
- [ ] On Enter key: check if input starts with `/`
- [ ] If command: handle locally (skip LLM)
- [ ] If message: add to conversation, transition to WAITING_FOR_LLM
- [ ] Start HTTP request via curl_multi
- [ ] Show spinner
- **Tests:** Integration test - Enter key triggers state change
- **Manual verification:** Cannot test end-to-end until Phase 7

#### Task 6.6: Wire streaming callback to scrollback
- [ ] Register callback that appends content to scrollback
- [ ] Each chunk triggers scrollback update
- [ ] Trigger viewport auto-scroll to bottom
- [ ] Re-render after each chunk
- **Tests:** Unit test - callback invoked, scrollback updated
- **Coverage:** Multiple chunks, large content

#### Task 6.7: Handle request completion
- [ ] On transfer complete: hide spinner
- [ ] Show input layer again
- [ ] Transition back to IDLE state
- [ ] Add assistant message to conversation history
- **Tests:** Unit test - completion handling
- **Coverage:** Success and error completion

#### Task 6.8: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review test outputs

### Phase 7: End-to-End Integration & Manual Testing

**Goal:** Complete integration and verify full user experience

#### Task 7.1: Implement /clear command
- [ ] Parse `/clear` command
- [ ] Clear scrollback buffer
- [ ] Clear conversation history
- [ ] Reset to empty state
- **Tests:** Unit test - verify scrollback and conversation cleared
- **Manual verification:** Run app, send messages, type `/clear`, verify UI cleared

#### Task 7.2: Implement /help command
- [ ] Parse `/help` command
- [ ] Show available commands in scrollback
- [ ] List: /clear, /help, /model, /system
- **Tests:** Unit test - verify help text
- **Manual verification:** Type `/help`, verify output

#### Task 7.3: Implement /model command
- [ ] Parse `/model <name>` command
- [ ] Validate model name (gpt-4-turbo, gpt-3.5-turbo, etc.)
- [ ] Update config in-memory
- [ ] Show confirmation message
- **Tests:** Unit test - valid/invalid model names
- **Manual verification:** Type `/model gpt-3.5-turbo`, verify switch

#### Task 7.4: Implement /system command
- [ ] Parse `/system <text>` command
- [ ] Update system_message in config
- [ ] Show confirmation
- **Tests:** Unit test - set/clear system message
- **Manual verification:** Type `/system You are a pirate`, verify personality change

#### Task 7.5: Error handling integration
- [ ] HTTP errors → format message, append to scrollback
- [ ] Parse errors → show error, return to IDLE
- [ ] Connection timeouts → show message
- **Tests:** Unit test with error fixtures
- **Manual verification:** Use invalid API key, verify error shown

#### Task 7.6: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass

#### Task 7.7: **MANUAL TESTING SESSION 1** (Basic functionality)
- [ ] **Human:** Set valid API key in `~/.config/ikigai/config.json`
- [ ] **Human:** Run `bin/ikigai`
- [ ] **Human:** Type "Hello!" and press Enter
- [ ] **Verify:** Spinner appears, animates at ~80ms intervals
- [ ] **Verify:** Response streams into scrollback
- [ ] **Verify:** Spinner disappears when done
- [ ] **Verify:** Input area returns
- [ ] **Human:** Type follow-up question "What's 2+2?"
- [ ] **Verify:** Conversation context maintained (assistant remembers previous message)
- [ ] **Document:** Any issues, unexpected behavior

#### Task 7.8: **MANUAL TESTING SESSION 2** (Multi-line & scrolling)
- [ ] **Human:** Type a multi-line message (Shift+Enter for newlines)
- [ ] **Verify:** Message wraps correctly in scrollback
- [ ] **Human:** Send long question that generates long response
- [ ] **Verify:** Viewport auto-scrolls as response streams in
- [ ] **Verify:** Can manually scroll up (Page Up) while streaming
- [ ] **Human:** Let response complete
- [ ] **Verify:** Can scroll through full history
- [ ] **Document:** Scrolling behavior, any issues

#### Task 7.9: **MANUAL TESTING SESSION 3** (Commands)
- [ ] **Human:** Type `/help`
- [ ] **Verify:** Help text appears in scrollback
- [ ] **Human:** Type `/model gpt-3.5-turbo`
- [ ] **Verify:** Model switched, confirmation shown
- [ ] **Human:** Send message, verify response from new model
- [ ] **Human:** Type `/system You are a helpful pirate assistant`
- [ ] **Verify:** System message set, personality changes
- [ ] **Human:** Type `/clear`
- [ ] **Verify:** Scrollback and conversation cleared
- [ ] **Document:** Command behavior

#### Task 7.10: **MANUAL TESTING SESSION 4** (Error handling)
- [ ] **Human:** Edit config with invalid API key
- [ ] **Human:** Send message
- [ ] **Verify:** 401 error shown in scrollback, input returns
- [ ] **Human:** Restore valid API key
- [ ] **Human:** Disconnect network (unplug or disable)
- [ ] **Human:** Send message
- [ ] **Verify:** Connection error shown gracefully
- [ ] **Human:** Restore network
- [ ] **Verify:** Next message works
- [ ] **Document:** Error messages, recovery behavior

#### Task 7.11: **MANUAL TESTING SESSION 5** (Stress testing)
- [ ] **Human:** Send 20+ messages back-to-back
- [ ] **Verify:** No memory leaks (monitor with `top` or `valgrind`)
- [ ] **Verify:** Performance remains smooth
- [ ] **Human:** Send very long message (multi-paragraph)
- [ ] **Verify:** Request succeeds, response handled
- [ ] **Human:** Request very long response ("write a 2000 word essay")
- [ ] **Verify:** Streaming handles large response
- [ ] **Verify:** Scrollback handles large content
- [ ] **Document:** Performance, any degradation

#### Task 7.12: Create manual test checklist document
- [ ] Compile all manual tests into `docs/manual_tests.md`
- [ ] Include expected behavior for each test
- [ ] Add troubleshooting section
- [ ] Document common issues and fixes
- **Manual verification:** Review document for completeness

### Phase 8: Mock Verification & Polish

**Goal:** Verify fixtures against real API, finalize implementation

#### Task 8.1: Create mock verification test suite
- [ ] Create `tests/integration/openai_mock_verification_test.c`
- [ ] Write test that skips if `VERIFY_MOCKS` not set
- [ ] Implement real API call for each fixture
- [ ] Compare structure (not exact content)
- [ ] Verify required fields present
- **Tests:** Run with `VERIFY_MOCKS=1` and valid API key
- **Manual verification:** Execute `OPENAI_API_KEY=sk-... VERIFY_MOCKS=1 make check`

#### Task 8.2: Update fixtures if needed
- [ ] Run verification tests
- [ ] If fixtures outdated, capture new responses
- [ ] Review for sensitive data, scrub
- [ ] Update fixture files
- [ ] Re-run regular tests to ensure compatibility
- **Manual verification:** Review diff of fixture changes

#### Task 8.3: Add Makefile target for verification
- [ ] Add `verify-mocks` target to Makefile
- [ ] Requires `OPENAI_API_KEY` environment variable
- [ ] Runs verification test suite
- [ ] Documents usage in comments
- **Manual verification:** Run `make verify-mocks` with valid key

#### Task 8.4: Documentation update
- [ ] Update `docs/README.md` with LLM integration status
- [ ] Mark "OpenAI API Integration" as complete
- [ ] Add section about mock verification workflow
- [ ] Document configuration fields
- **Manual verification:** Review documentation for accuracy

#### Task 8.5: Final quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0% (all metrics)
- [ ] Run `make check-dynamic` - all pass (ASan, UBSan, TSan)
- [ ] Run `make check-valgrind` - no leaks, no errors
- **Manual verification:** All quality gates green

#### Task 8.6: **FINAL MANUAL ACCEPTANCE TEST**
- [ ] **Human:** Fresh build (`make clean && make`)
- [ ] **Human:** Delete config, let it create defaults
- [ ] **Human:** Set valid API key
- [ ] **Human:** Run through all manual test sessions (7.7-7.11)
- [ ] **Verify:** All behaviors correct
- [ ] **Verify:** No crashes, no errors
- [ ] **Verify:** Performance acceptable
- [ ] **Decision:** ACCEPT or list remaining issues

## Success Criteria

Implementation is complete when:

1. ✅ All unit tests pass (100% coverage)
2. ✅ All integration tests pass
3. ✅ All quality gates pass (fmt, lint, check-dynamic, valgrind)
4. ✅ All manual test sessions completed successfully
5. ✅ Mock verification tests pass against real API
6. ✅ Documentation updated
7. ✅ Final acceptance test passed by human

## Notes

- Each phase builds on previous phases
- Can only proceed to next phase when current phase complete
- Manual verification points are critical - do not skip
- If any test fails, fix before proceeding
- Maintain 100% test coverage throughout
- Never commit with failing tests or quality gates
