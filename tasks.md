# Implementation Tasks (Phase 1: HTTP LLM Integration)

**Scope:** This document contains detailed implementation tasks for Phase 1 - basic HTTP LLM integration with in-memory state.

**Architecture Reference:** See `plan.md` for architectural decisions and user experience flow.

**Future Phases** (see `docs/logical-architecture-analysis.md`):
- Phase 2: Database Integration (sessions, message persistence)
- Phase 3: Tool Execution (search, file ops, shell commands)
- Phase 4: Multi-LLM Support (Anthropic, Google, X.AI)

## Phase 1.1: Configuration Extension

**Goal:** Add OpenAI configuration fields to existing config module

### Task 1.1: Extend ik_cfg_t structure
- [ ] Add `openai_model` field (char *)
- [ ] Add `openai_temperature` field (double)
- [ ] Add `openai_max_tokens` field (int32_t)
- [ ] Add `openai_system_message` field (char *, nullable)
- [ ] Update header file `src/config.h`
- **Tests:** Compile-only (structure change)

### Task 1.2: Update default config creation
- [ ] Add default values to `create_default_config()` JSON
- [ ] Set `openai_model` = "gpt-4-turbo"
- [ ] Set `openai_temperature` = 0.7
- [ ] Set `openai_max_tokens` = 4096
- [ ] Set `openai_system_message` = null
- **Tests:** Integration test verifies default config file contains new fields
- **Manual verification:** Delete `~/.config/ikigai/config.json`, run `make test`, verify defaults

### Task 1.3: Add parsing for new fields
- [ ] Parse `openai_model` field (required, string)
- [ ] Parse `openai_temperature` field (required, number, range 0.0-2.0)
- [ ] Parse `openai_max_tokens` field (required, integer, range 1-128000)
- [ ] Parse `openai_system_message` field (optional, string or null)
- [ ] Add validation for ranges
- **Tests:** Unit tests for valid/invalid values, missing fields, type errors
- **Coverage:** OOM injection on all allocation paths

### Task 1.4: Update integration tests
- [ ] Test loading config with all new fields
- [ ] Test validation errors (temperature out of range, etc.)
- [ ] Test optional system_message (null and string values)
- **Tests:** `make check` passes with 100% coverage
- **Manual verification:** Review test output

### Task 1.5: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0% on all metrics
- [ ] Run `make check-dynamic` - all sanitizers pass
- **Manual verification:** Review all quality gate outputs

## Phase 1.2: Layer Abstraction Foundation

**Goal:** Create layer abstraction API and refactor existing rendering

### Task 2.1: Design layer interface
- [ ] Create `src/layer.h` with `ik_layer_t` structure
- [ ] Define `ik_layer_is_visible_fn` function pointer type
- [ ] Define `ik_layer_get_height_fn` function pointer type
- [ ] Define `ik_layer_render_fn` function pointer type
- [ ] Document layer contract in comments
- **Tests:** Header-only, compile verification

### Task 2.2: Create layer constructor/destructor
- [ ] Write `ik_layer_create()` function
- [ ] Write `ik_layer_destroy()` function (talloc-based)
- [ ] Add precondition assertions
- **Tests:** Unit test - create/destroy layer, verify talloc hierarchy
- **Coverage:** OOM injection

### Task 2.3: Create layer cake manager structure
- [ ] Create `src/layer_cake.h` with `ik_layer_cake_t` structure
- [ ] Add `layers` array field
- [ ] Add `layer_count`, `viewport_row`, `viewport_height` fields
- [ ] Write `ik_layer_cake_create()` function
- [ ] Write `ik_layer_cake_destroy()` function
- **Tests:** Unit test - create/destroy cake
- **Coverage:** OOM injection

### Task 2.4: Implement ik_layer_cake_add_layer()
- [ ] Write function to append layer to cake
- [ ] Resize layers array as needed
- [ ] Maintain layer ordering (top to bottom)
- **Tests:** Unit test - add 1, 2, 4 layers, verify order
- **Coverage:** OOM injection on array resize

### Task 2.5: Implement ik_layer_cake_get_total_height()
- [ ] Iterate through all layers
- [ ] Call `is_visible()` for each layer
- [ ] Sum `get_height()` for visible layers only
- [ ] Return total height
- **Tests:** Unit test with mock layers (different visibility combinations)

### Task 2.6: Implement ik_layer_cake_render()
- [ ] Calculate which layers are in viewport range
- [ ] Call `render()` on each visible layer in viewport
- [ ] Handle partial layer rendering (viewport cuts through layer)
- [ ] Accumulate output
- **Tests:** Unit test with mock layers, verify correct layers rendered
- **Coverage:** Edge cases (viewport at top, bottom, middle)

### Task 2.7: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review outputs

## Phase 1.3: Refactor Existing Rendering to Layers

**Goal:** Wrap existing scrollback, separator, input in layer abstraction

### Task 3.1: Create scrollback layer wrapper
- [ ] Write `ik_scrollback_layer_create()` function
- [ ] Implement `scrollback_is_visible()` (always true)
- [ ] Implement `scrollback_get_height()` (delegate to existing scrollback)
- [ ] Implement `scrollback_render()` (delegate to existing render function)
- [ ] Store `ik_scrollback_t*` in layer's `data` pointer
- **Tests:** Unit test - create scrollback layer, verify delegation
- **Coverage:** OOM injection

### Task 3.2: Create separator layer wrapper
- [ ] Write `ik_separator_layer_create()` function
- [ ] Implement `separator_is_visible()` (always true)
- [ ] Implement `separator_get_height()` (always 1)
- [ ] Implement `separator_render()` (render separator line)
- **Tests:** Unit test - render separator at various widths
- **Coverage:** OOM injection

### Task 3.3: Create input layer wrapper
- [ ] Write `ik_input_layer_create()` function
- [ ] Implement `input_is_visible()` (check REPL state)
- [ ] Implement `input_get_height()` (delegate to existing input buffer)
- [ ] Implement `input_render()` (delegate to existing render function)
- [ ] Store `ik_input_buf_t*` in layer's `data` pointer
- **Tests:** Unit test - visibility changes based on state
- **Coverage:** OOM injection, state variations

### Task 3.4: Integrate layers into REPL
- [ ] Create `ik_layer_cake_t` in REPL context
- [ ] Add scrollback layer to cake
- [ ] Add separator layer to cake
- [ ] Add input layer to cake
- [ ] Verify layer ordering
- **Tests:** Integration test - REPL creates cake with 3 layers
- **Coverage:** OOM during REPL initialization

### Task 3.5: Replace REPL render with layer_cake_render
- [ ] Remove old direct rendering code
- [ ] Call `ik_layer_cake_render()` instead
- [ ] Verify output identical to before
- **Tests:** Regression tests - output matches old implementation
- **Manual verification:** Run `bin/ikigai`, verify UI looks identical

### Task 3.6: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Interactive REPL testing (multi-line input, scrolling, etc.)

## Phase 1.4: Spinner Layer

**Goal:** Add animated spinner layer for LLM wait state

### Task 4.1: Create spinner state structure
- [ ] Define `ik_spinner_state_t` structure
- [ ] Add `frame_index` field (which animation frame)
- [ ] Add `visible` flag
- [ ] Write `ik_spinner_state_create()` function
- **Tests:** Unit test - create spinner state
- **Coverage:** OOM injection

### Task 4.2: Implement spinner animation frames
- [ ] Define spinner frames: `|`, `/`, `-`, `\`
- [ ] Write `ik_spinner_get_frame()` function (returns current char)
- [ ] Write `ik_spinner_advance()` function (cycles frame_index)
- **Tests:** Unit test - cycle through all 4 frames
- **Coverage:** Full frame cycle

### Task 4.3: Create spinner layer wrapper
- [ ] Write `ik_spinner_layer_create()` function
- [ ] Implement `spinner_is_visible()` (check spinner state)
- [ ] Implement `spinner_get_height()` (1 if visible, 0 if hidden)
- [ ] Implement `spinner_render()` (render current frame with message)
- [ ] Store `ik_spinner_state_t*` in layer's `data` pointer
- **Tests:** Unit test - visibility, height, rendering
- **Coverage:** OOM injection

### Task 4.4: Add spinner to layer cake
- [ ] Insert spinner layer between scrollback and separator
- [ ] Verify layer ordering: scrollback, spinner, separator, input
- **Tests:** Integration test - layer cake has 4 layers in correct order
- **Manual verification:** Inspect layer cake structure

### Task 4.5: Add REPL state for spinner control
- [ ] Add `WAITING_FOR_LLM` state to REPL state enum
- [ ] Add spinner visibility toggle function
- [ ] Add spinner frame advance function (called on timer)
- **Tests:** Unit test - state transitions, visibility changes
- **Coverage:** All state transitions

### Task 4.6: Add timer event for spinner animation
- [ ] Add 80ms timer to event loop (only when WAITING_FOR_LLM)
- [ ] Timer callback advances spinner frame
- [ ] Trigger re-render after each frame advance
- **Tests:** Unit test - timer triggers, frame advances, render called
- **Manual verification:** Can be tested manually in Phase 6

### Task 4.7: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review test outputs

## Phase 1.5: HTTP Client Module (libcurl)

**Goal:** Create OpenAI HTTP client with streaming support

### Task 5.1: Create module structure
- [ ] Create `src/openai/` directory
- [ ] Create `src/openai/client.h` header
- [ ] Create `src/openai/client.c` implementation
- [ ] Add to build system (Makefile)
- **Tests:** Compile-only verification

### Task 5.2: Define OpenAI message structures
- [ ] Define `ik_openai_msg_t` structure (role, content)
- [ ] Define `ik_openai_conversation_t` structure (messages array)
- [ ] Write `ik_openai_msg_create()` function
- [ ] Write `ik_openai_conversation_create()` function
- [ ] Write `ik_openai_conversation_add_message()` function
- **Tests:** Unit test - create messages, build conversation
- **Coverage:** OOM injection on all allocations

### Task 5.3: Define request/response structures
- [ ] Define `ik_openai_request_t` structure (model, messages, temperature, etc.)
- [ ] Define `ik_openai_response_t` structure (content, finish_reason, usage)
- [ ] Write constructor/destructor functions
- **Tests:** Unit test - create/destroy structures
- **Coverage:** OOM injection

### Task 5.4: Implement JSON request serialization
- [ ] Write `ik_openai_serialize_request()` function
- [ ] Use yyjson to build JSON request body
- [ ] Include model, messages array, temperature, max_tokens, stream=true
- [ ] Format according to OpenAI API spec
- **Tests:** Unit test - serialize request, verify JSON structure
- **Coverage:** OOM injection in yyjson allocations

### Task 5.5: Create test fixtures
- [ ] Create `tests/fixtures/openai/stream_hello_world.txt`
- [ ] Create `tests/fixtures/openai/stream_multiline.txt`
- [ ] Create `tests/fixtures/openai/stream_done.txt`
- [ ] Create `tests/fixtures/openai/error_401_unauthorized.json`
- [ ] Create `tests/fixtures/openai/error_429_rate_limit.json`
- [ ] Create `tests/fixtures/openai/error_500_server.json`
- **Manual step:** Make real API calls to capture responses
- **Manual verification:** Review fixtures for sensitive data, scrub if needed

### Task 5.6: Implement SSE parser (buffered line reader)
- [ ] Write `ik_openai_sse_parser_create()` function
- [ ] Implement `ik_openai_sse_parser_feed()` - accumulate bytes
- [ ] Detect `\n\n` delimiter (complete SSE event)
- [ ] Extract complete events, keep incomplete data in buffer
- **Tests:** Unit test - feed partial data, verify buffering
- **Coverage:** Various buffer sizes, edge cases

### Task 5.7: Implement SSE event parsing
- [ ] Write `ik_openai_parse_sse_event()` function
- [ ] Strip `data: ` prefix
- [ ] Handle `data: [DONE]` marker (end of stream)
- [ ] Parse JSON chunk using yyjson
- [ ] Extract `choices[0].delta.content` field
- [ ] Return content string or NULL if [DONE]
- **Tests:** Unit test - parse fixture events, verify content extraction
- **Coverage:** Malformed events, missing fields, [DONE] marker

### Task 5.8: Implement libcurl HTTP client (synchronous first)
- [ ] Write `ik_openai_http_post()` function
- [ ] Initialize libcurl easy handle
- [ ] Set URL, headers (Authorization, Content-Type)
- [ ] Set request body
- [ ] Set write callback for response data
- [ ] Execute synchronous request
- [ ] Return response body
- **Tests:** Unit test with mock server or fixtures
- **Coverage:** OOM injection, libcurl errors

### Task 5.9: Add streaming support to HTTP client
- [ ] Modify write callback to feed SSE parser incrementally
- [ ] Call callback for each extracted content chunk
- [ ] Handle partial responses
- **Tests:** Unit test - feed fixture data in chunks, verify callbacks
- **Coverage:** Various chunk sizes, connection errors

### Task 5.10: Implement ik_openai_chat_create() (high-level API)
- [ ] Write main API function
- [ ] Takes conversation, config, callback function
- [ ] Serializes request
- [ ] Makes HTTP call with streaming
- [ ] Invokes callback for each content chunk
- [ ] Returns complete response
- **Tests:** Unit test with fixtures - verify end-to-end flow
- **Coverage:** All error paths (HTTP errors, parse errors, etc.)

### Task 5.11: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review all test outputs

## Phase 1.6: Event Loop Integration (Non-blocking I/O)

**Goal:** Integrate libcurl multi interface with REPL event loop

### Task 6.1: Refactor HTTP client to use curl_multi
- [ ] Replace `curl_easy_perform()` with `curl_multi_*` API
- [ ] Write `ik_openai_client_multi_create()` function
- [ ] Write `ik_openai_client_multi_add_request()` function
- [ ] Write `ik_openai_client_multi_perform()` function
- [ ] Return file descriptors for select() integration
- **Tests:** Unit test - verify non-blocking behavior
- **Coverage:** curl_multi errors, handle management

### Task 6.2: Add curl FDs to REPL select() call
- [ ] Call `curl_multi_fdset()` to get read/write/except fd sets
- [ ] Merge with existing terminal fd in select()
- [ ] Calculate correct timeout (min of timer timeout and curl timeout)
- **Tests:** Integration test - mock curl FDs, verify select() behavior
- **Coverage:** FD set handling, timeout calculation

### Task 6.3: Add curl_multi_perform() to event loop
- [ ] Check if curl FDs are ready after select()
- [ ] Call `curl_multi_perform()` if ready
- [ ] Process completed transfers
- [ ] Invoke callbacks for streaming chunks
- **Tests:** Integration test - simulate ready FDs, verify perform called
- **Coverage:** Transfer completion, errors

### Task 6.4: Add REPL state machine transitions
- [ ] Add `IDLE` state (normal input)
- [ ] Add `WAITING_FOR_LLM` state (spinner visible, input hidden)
- [ ] Implement state transition on Enter key (IDLE → WAITING_FOR_LLM)
- [ ] Implement state transition on response complete (WAITING_FOR_LLM → IDLE)
- **Tests:** Unit test - verify state transitions
- **Coverage:** All transition paths

### Task 6.5: Wire Enter key to API request
- [ ] On Enter key: check if input starts with `/`
- [ ] If command: handle locally (skip LLM)
- [ ] If message: add to conversation, transition to WAITING_FOR_LLM
- [ ] Start HTTP request via curl_multi
- [ ] Show spinner
- **Tests:** Integration test - Enter key triggers state change
- **Manual verification:** Cannot test end-to-end until Phase 7

### Task 6.6: Wire streaming callback to scrollback
- [ ] Register callback that appends content to scrollback
- [ ] Each chunk triggers scrollback update
- [ ] Trigger viewport auto-scroll to bottom
- [ ] Re-render after each chunk
- **Tests:** Unit test - callback invoked, scrollback updated
- **Coverage:** Multiple chunks, large content

### Task 6.7: Handle request completion
- [ ] On transfer complete: hide spinner
- [ ] Show input layer again
- [ ] Transition back to IDLE state
- [ ] Add assistant message to conversation history
- **Tests:** Unit test - completion handling
- **Coverage:** Success and error completion

### Task 6.8: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass
- **Manual verification:** Review test outputs

## Phase 1.7: Command Infrastructure & Manual Testing

**Goal:** Implement command registry and essential commands

### Task 7.1: Create command registry
- [ ] Create `src/commands.c` and `src/commands.h`
- [ ] Define `ik_cmd_handler_t` function signature
- [ ] Define `ik_command_t` structure (name, description, handler)
- [ ] Implement command registry array
- [ ] Implement `dispatch_command()` function
- **Tests:** Unit test - dispatch to correct handler
- **Coverage:** Unknown command handling

### Task 7.2: Implement /clear command
- [ ] Clear scrollback buffer
- [ ] Clear session_messages[] array (free all messages)
- [ ] Clear marks[] array (free all marks)
- [ ] Reset counters to 0
- **Tests:** Unit test - verify scrollback, session messages, and marks cleared
- **Manual verification:** Run app, send messages, create marks, type `/clear`, verify UI cleared

### Task 7.3: Implement /mark command
- [ ] Parse `/mark [label]` command
- [ ] Create `ik_message_t` with role="mark" and content=label
- [ ] Create `ik_mark_t` structure at current position
- [ ] Add mark to marks[] array
- [ ] Add mark message to session_messages[]
- [ ] Render mark indicator in scrollback
- **Tests:** Unit test - mark creation, multiple marks, labeled/unlabeled marks
- **Coverage:** OOM injection on mark allocation

### Task 7.4: Implement /rewind command
- [ ] Parse `/rewind [label]` command
- [ ] Find target mark (most recent if no label, matching label otherwise)
- [ ] Truncate session_messages[] to mark position
- [ ] Remove marks at and after target
- [ ] Rebuild scrollback from remaining messages
- [ ] Render rewind indicator in scrollback
- **Tests:** Unit test - rewind to unnamed mark, labeled mark, no marks error
- **Coverage:** Various mark configurations

### Task 7.5: Implement /help command
- [ ] Auto-generate help text from command registry
- [ ] Show available commands with descriptions
- [ ] Append to scrollback
- **Tests:** Unit test - verify help text includes all registered commands
- **Manual verification:** Type `/help`, verify output

### Task 7.6: Implement /model command
- [ ] Parse `/model <name>` command
- [ ] Validate model name (gpt-4-turbo, gpt-3.5-turbo, etc.)
- [ ] Update config in-memory
- [ ] Show confirmation message
- **Tests:** Unit test - valid/invalid model names
- **Manual verification:** Type `/model gpt-3.5-turbo`, verify switch

### Task 7.7: Implement /system command
- [ ] Parse `/system <text>` command
- [ ] Update system_message in config
- [ ] Show confirmation
- **Tests:** Unit test - set/clear system message
- **Manual verification:** Type `/system You are a pirate`, verify personality change

### Task 7.8: Error handling integration
- [ ] HTTP errors → format message, append to scrollback
- [ ] Parse errors → show error, return to IDLE
- [ ] Connection timeouts → show message
- **Tests:** Unit test with error fixtures
- **Manual verification:** Use invalid API key, verify error shown

### Task 7.9: Quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0%
- [ ] Run `make check-dynamic` - all pass

### Task 7.10: **MANUAL TESTING SESSION 1** (Basic functionality)
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

### Task 7.11: **MANUAL TESTING SESSION 2** (Multi-line & scrolling)
- [ ] **Human:** Type a multi-line message (Shift+Enter for newlines)
- [ ] **Verify:** Message wraps correctly in scrollback
- [ ] **Human:** Send long question that generates long response
- [ ] **Verify:** Viewport auto-scrolls as response streams in
- [ ] **Verify:** Can manually scroll up (Page Up) while streaming
- [ ] **Human:** Let response complete
- [ ] **Verify:** Can scroll through full history
- [ ] **Document:** Scrolling behavior, any issues

### Task 7.12: **MANUAL TESTING SESSION 3** (Commands)
- [ ] **Human:** Type `/help`
- [ ] **Verify:** Help text appears in scrollback, includes all commands
- [ ] **Human:** Type `/model gpt-3.5-turbo`
- [ ] **Verify:** Model switched, confirmation shown
- [ ] **Human:** Send message, verify response from new model
- [ ] **Human:** Type `/system You are a helpful pirate assistant`
- [ ] **Verify:** System message set, personality changes
- [ ] **Human:** Type `/mark checkpoint1`
- [ ] **Verify:** Mark indicator appears in scrollback
- [ ] **Human:** Send a few more messages
- [ ] **Human:** Type `/rewind checkpoint1`
- [ ] **Verify:** Scrollback truncated to mark, later messages removed
- [ ] **Human:** Type `/clear`
- [ ] **Verify:** Scrollback, session messages, and marks all cleared
- [ ] **Document:** Command behavior

### Task 7.13: **MANUAL TESTING SESSION 4** (Error handling)
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

### Task 7.14: **MANUAL TESTING SESSION 5** (Stress testing)
- [ ] **Human:** Send 20+ messages back-to-back
- [ ] **Verify:** No memory leaks (monitor with `top` or `valgrind`)
- [ ] **Verify:** Performance remains smooth
- [ ] **Human:** Send very long message (multi-paragraph)
- [ ] **Verify:** Request succeeds, response handled
- [ ] **Human:** Request very long response ("write a 2000 word essay")
- [ ] **Verify:** Streaming handles large response
- [ ] **Verify:** Scrollback handles large content
- [ ] **Document:** Performance, any degradation

### Task 7.15: Create manual test checklist document
- [ ] Compile all manual tests into `docs/manual_tests.md`
- [ ] Include expected behavior for each test
- [ ] Add troubleshooting section
- [ ] Document common issues and fixes
- **Manual verification:** Review document for completeness

## Phase 1.8: Mock Verification & Polish

**Goal:** Verify fixtures against real API, finalize Phase 1 implementation

### Task 8.1: Create mock verification test suite
- [ ] Create `tests/integration/openai_mock_verification_test.c`
- [ ] Write test that skips if `VERIFY_MOCKS` not set
- [ ] Implement real API call for each fixture
- [ ] Compare structure (not exact content)
- [ ] Verify required fields present
- **Tests:** Run with `VERIFY_MOCKS=1` and valid API key
- **Manual verification:** Execute `OPENAI_API_KEY=sk-... VERIFY_MOCKS=1 make check`

### Task 8.2: Update fixtures if needed
- [ ] Run verification tests
- [ ] If fixtures outdated, capture new responses
- [ ] Review for sensitive data, scrub
- [ ] Update fixture files
- [ ] Re-run regular tests to ensure compatibility
- **Manual verification:** Review diff of fixture changes

### Task 8.3: Add Makefile target for verification
- [ ] Add `verify-mocks` target to Makefile
- [ ] Requires `OPENAI_API_KEY` environment variable
- [ ] Runs verification test suite
- [ ] Documents usage in comments
- **Manual verification:** Run `make verify-mocks` with valid key

### Task 8.4: Documentation update
- [ ] Update `docs/README.md` with LLM integration status
- [ ] Mark "OpenAI API Integration" as complete
- [ ] Add section about mock verification workflow
- [ ] Document configuration fields
- **Manual verification:** Review documentation for accuracy

### Task 8.5: Final quality gates
- [ ] Run `make fmt`
- [ ] Run `make check` - 100% pass
- [ ] Run `make lint` - all pass
- [ ] Run `make coverage` - 100.0% (all metrics)
- [ ] Run `make check-dynamic` - all pass (ASan, UBSan, TSan)
- [ ] Run `make check-valgrind` - no leaks, no errors
- **Manual verification:** All quality gates green

### Task 8.6: **FINAL MANUAL ACCEPTANCE TEST**
- [ ] **Human:** Fresh build (`make clean && make`)
- [ ] **Human:** Delete config, let it create defaults
- [ ] **Human:** Set valid API key
- [ ] **Human:** Run through all manual test sessions (7.10-7.14)
- [ ] **Verify:** All behaviors correct (streaming, commands, /mark, /rewind)
- [ ] **Verify:** No crashes, no errors
- [ ] **Verify:** Performance acceptable
- [ ] **Decision:** ACCEPT Phase 1 or list remaining issues

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

## Notes

- Each sub-phase builds on previous sub-phases
- Can only proceed to next sub-phase when current sub-phase complete
- Manual verification points are critical - do not skip
- If any test fails, fix before proceeding
- Maintain 100% test coverage throughout
- Never commit with failing tests or quality gates
- Database persistence deferred to Phase 2 - acceptable to lose messages on exit for Phase 1
