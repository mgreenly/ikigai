# Implementation Tasks (Phase 1: HTTP LLM Integration)

**Scope:** This document contains detailed implementation tasks for Phase 1 - basic HTTP LLM integration with in-memory state.

**Architecture Reference:** See `plan.md` for architectural decisions and user experience flow.

**Overall Progress:**
- ✅ Phase 1.1: Configuration Extension - COMPLETE
- ✅ Phase 1.2: Layer Abstraction Foundation - COMPLETE
- ✅ Phase 1.3: Refactor Existing Rendering to Layers - COMPLETE
- ✅ Phase 1.4: Spinner Layer - COMPLETE
- ✅ Phase 1.5: HTTP Client Module (libcurl) - COMPLETE
- ✅ Phase 1.6: Event Loop Integration - COMPLETE
- ⏳ Phase 1.7: Command Infrastructure & Manual Testing - IN PROGRESS (9/16 tasks)
- ⏳ Phase 1.8: Mock Verification & Polish - PENDING

**Future Phases** (see `docs/logical-architecture-analysis.md`):
- Phase 2: Database Integration (sessions, message persistence)
- Phase 3: Tool Execution (search, file ops, shell commands)
- Phase 4: Multi-LLM Support (Anthropic, Google, X.AI)

## Completed Tasks

**Phase 1.1: Configuration Extension** ✅
- Task 1.1: Extended ik_cfg_t structure with OpenAI fields
- Task 1.2: Updated default config creation
- Task 1.3: Added parsing for new fields with validation
- Task 1.4: Updated integration tests
- Task 1.5: Quality gates passed

**Phase 1.2: Layer Abstraction Foundation** ✅
- Task 2.1: Designed layer interface
- Task 2.2: Created layer constructor/destructor
- Task 2.3: Created layer cake manager structure
- Task 2.4: Implemented ik_layer_cake_add_layer()
- Task 2.5: Implemented ik_layer_cake_get_total_height()
- Task 2.6: Implemented ik_layer_cake_render()
- Task 2.7: Quality gates passed

**Phase 1.3: Refactor Existing Rendering to Layers** ✅
- Task 3.1: Created scrollback layer wrapper
- Task 3.2: Created separator layer wrapper
- Task 3.3: Created input layer wrapper
- Task 3.4: Integrated layers into REPL
- Task 3.5: Replaced REPL render with layer_cake_render
- Task 3.6: Quality gates passed

**Phase 1.4: Spinner Layer** ✅
- Task 4.1: Created spinner state structure
- Task 4.2: Implemented spinner animation frames
- Task 4.3: Created spinner layer wrapper
- Task 4.4: Added spinner to layer cake
- Task 4.5: REPL state for spinner control (completed in Phase 1.6)
- Task 4.6: Timer event for spinner animation (completed in Phase 1.6)
- Task 4.7: Quality gates passed

**Phase 1.5: HTTP Client Module (libcurl)** ✅
- Task 5.1: Created module structure
- Task 5.2: Defined OpenAI message structures
- Task 5.3: Defined request/response structures
- Task 5.4: Implemented JSON request serialization
- Task 5.5: Created test fixtures
- Task 5.6: Implemented SSE parser (buffered line reader)
- Task 5.7: Implemented SSE event parsing
- Task 5.8: Achieved 100% coverage
- Task 5.9: Implemented libcurl HTTP client
- Task 5.10: Added streaming support to HTTP client
- Task 5.11: Implemented ik_openai_chat_create() (high-level API)
- Task 5.12: Quality gates passed

**Phase 1.6: Event Loop Integration** ✅
- Task 6.1: Refactored HTTP client to use curl_multi
- Task 6.2: Added curl FDs to REPL select() call
- Task 6.3: Added curl_multi_perform() to event loop
- Task 6.4: Added REPL state machine transitions
- Task 6.5: Wired Enter key to API request
- Task 6.6: Wired streaming callback to scrollback
- Task 6.7: Handled request completion
- Task 6.8: Quality gates passed

**Phase 1.7: Command Infrastructure** ✅ (Tasks 7.1-7.9)
- Task 7.1: Created command registry
- Task 7.1b: Fixed command dispatch tests
- Task 7.2: Implemented /clear command
- Task 7.3: Implemented /mark command
- Task 7.4: Implemented /rewind command
- Task 7.5: Implemented /help command
- Task 7.6: Implemented /model command
- Task 7.7: Implemented /system command
- Task 7.8: Error handling integration
- Task 7.9: Quality gates passed (100% coverage: 2975 lines, 210 functions, 932 branches)

---

## Phase 1.7: Manual Testing (Pending)

**Goal:** Validate implementation through human testing

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

1. ✅ All unit tests pass (100% coverage) - DONE
2. ✅ All integration tests pass - DONE
3. ✅ All quality gates pass (fmt, lint, check-dynamic, valgrind) - DONE
4. ⏳ All manual test sessions completed successfully - PENDING (Tasks 7.10-7.14)
5. ⏳ Mock verification tests pass against real API - PENDING (Phase 1.8)
6. ⏳ Documentation updated - PENDING (Phase 1.8)
7. ⏳ Final acceptance test passed by human - PENDING (Task 8.6)
8. ✅ Basic LLM chat works with streaming responses - DONE (Phase 1.6)
9. ✅ Commands work: /clear, /mark, /rewind, /help, /model, /system - DONE (all 6 commands implemented)
10. ✅ Messages stored in-memory only (database is Phase 2) - DONE

## Notes

- Manual verification points are critical - do not skip
- If any test fails, fix before proceeding
- Database persistence deferred to Phase 2 - acceptable to lose messages on exit for Phase 1

