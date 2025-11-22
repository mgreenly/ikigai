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
- ✅ Phase 1.7: Command Infrastructure & Manual Testing - COMPLETE
- ✅ Phase 1.8: Mock Verification & Polish - COMPLETE

**🎉 ALL PHASES COMPLETE 🎉**

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

## Phase 1.7: Manual Testing (Mostly Complete)

**Goal:** Validate implementation through human testing

### Task 7.10: **MANUAL TESTING SESSION 1** (Basic functionality) ✅
- [x] **Human:** Set valid API key in `~/.config/ikigai/config.json`
- [x] **Human:** Run `bin/ikigai`
- [x] **Human:** Type "Hello!" and press Enter
- [x] **Verify:** Spinner appears, animates at ~80ms intervals
- [x] **Verify:** Response streams into scrollback
- [x] **Verify:** Spinner disappears when done
- [x] **Verify:** Input area returns
- [x] **Human:** Type follow-up question "What's 2+2?"
- [x] **Verify:** Conversation context maintained (assistant remembers previous message)
- [x] **Document:** Any issues, unexpected behavior

### Task 7.11: **MANUAL TESTING SESSION 2** (Multi-line & scrolling) ✅
- [x] **Human:** Type a multi-line message (Shift+Enter for newlines)
- [x] **Verify:** Message wraps correctly in scrollback
- [x] **Human:** Send long question that generates long response
- [x] **Verify:** Viewport auto-scrolls as response streams in
- [x] **Verify:** Can manually scroll up (Page Up) while streaming
- [x] **Human:** Let response complete
- [x] **Verify:** Can scroll through full history
- [x] **Document:** Scrolling behavior, any issues

### Task 7.12: **MANUAL TESTING SESSION 3** (Commands) ✅
- [x] **Human:** Type `/help`
- [x] **Verify:** Help text appears in scrollback, includes all commands
- [x] **Human:** Type `/model gpt-3.5-turbo`
- [x] **Verify:** Model switched, confirmation shown
- [x] **Human:** Send message, verify response from new model
- [x] **Human:** Type `/system You are a helpful pirate assistant`
- [x] **Verify:** System message set, personality changes
- [x] **Human:** Type `/mark checkpoint1`
- [x] **Verify:** Mark indicator appears in scrollback
- [x] **Human:** Send a few more messages
- [x] **Human:** Type `/rewind checkpoint1`
- [x] **Verify:** Scrollback truncated to mark, later messages removed
- [x] **Human:** Type `/clear`
- [x] **Verify:** Scrollback, session messages, and marks all cleared
- [x] **Document:** Command behavior

### Task 7.13: **MANUAL TESTING SESSION 4** (Error handling) ✅
- [x] **Human:** Edit config with invalid API key
- [x] **Human:** Send message
- [x] **Verify:** 401 error shown in scrollback, input returns
- [x] **Human:** Restore valid API key
- [x] **Human:** Disconnect network (unplug or disable)
- [x] **Human:** Send message
- [x] **Verify:** Connection error shown gracefully
- [x] **Human:** Restore network
- [x] **Verify:** Next message works
- [x] **Document:** Error messages, recovery behavior

### Task 7.14: **MANUAL TESTING SESSION 5** (Stress testing) ✅
- [x] **Human:** Send 20+ messages back-to-back
- [x] **Verify:** No memory leaks (monitor with `top` or `valgrind`)
- [x] **Verify:** Performance remains smooth
- [x] **Human:** Send very long message (multi-paragraph)
- [x] **Verify:** Request succeeds, response handled
- [x] **Human:** Request very long response ("write a 2000 word essay")
- [x] **Verify:** Streaming handles large response
- [x] **Verify:** Scrollback handles large content
- [x] **Document:** Performance, any degradation

### Task 7.15: Create manual test checklist document ✅
- [x] Compile all manual tests into `docs/manual_tests.md`
- [x] Include expected behavior for each test
- [x] Add troubleshooting section
- [x] Document common issues and fixes
- **Manual verification:** Review document for completeness

## Phase 1.8: Mock Verification & Polish ✅ COMPLETE

**Goal:** Verify fixtures against real API, finalize Phase 1 implementation

**Status:** All tasks completed successfully

### Task 8.1: Create mock verification test suite ✅
- [x] Create `tests/integration/openai_mock_verification_test.c`
- [x] Write test that skips if `VERIFY_MOCKS` not set
- [x] Implement real API call for each fixture
- [x] Compare structure (not exact content)
- [x] Verify required fields present
- **Tests:** Run with `VERIFY_MOCKS=1` and valid API key
- **Manual verification:** Execute `OPENAI_API_KEY=sk-... VERIFY_MOCKS=1 make check`

### Task 8.2: Update fixtures if needed ✅
- [x] Run verification tests
- [x] Fixtures validated - no updates needed
- [x] All fixtures match real API structure
- **Manual verification:** 100% pass rate against real API

### Task 8.3: Add Makefile target for verification ✅
- [x] Add `verify-mocks` target to Makefile
- [x] Requires `OPENAI_API_KEY` environment variable
- [x] Runs verification test suite
- [x] Documents usage in comments
- **Manual verification:** Run `make verify-mocks` with valid key

### Task 8.4: Documentation update ✅
- [x] Update `docs/README.md` with LLM integration status
- [x] Mark "OpenAI API Integration" as complete
- [x] Add section about mock verification workflow
- [x] Document configuration fields
- **Manual verification:** Review documentation for accuracy

### Task 8.5: Final quality gates ✅
- [x] Run `make fmt`
- [x] Run `make check` - 100% pass
- [x] Run `make lint` - all pass (minor file size warnings tracked in fix.md)
- [x] Run `make coverage` - 100.0% (3178 lines, 219 functions, 1018 branches)
- [x] Run `make check-dynamic` - all pass (ASan, UBSan, TSan)
- [x] Run `make check-valgrind` - no leaks, no errors (125/125 tests pass)
- **Manual verification:** All quality gates green

### Task 8.6: **FINAL MANUAL ACCEPTANCE TEST** ✅
- [x] **Human:** Fresh build (`make clean && make`)
- [x] **Human:** Delete config, let it create defaults
- [x] **Human:** Set valid API key
- [x] **Human:** Run through all manual test sessions (7.10-7.14)
- [x] **Verify:** All behaviors correct (streaming, commands, /mark, /rewind)
- [x] **Verify:** No crashes, no errors
- [x] **Verify:** Performance acceptable
- [x] **Decision:** ACCEPT Phase 1 - `verify-mocks` verified working (100% pass with real API)

## Success Criteria (Phase 1)

Phase 1 implementation is complete when:

1. ✅ All unit tests pass (100% coverage) - DONE
2. ✅ All integration tests pass - DONE
3. ✅ All quality gates pass (fmt, lint, check-dynamic, valgrind) - DONE
4. ✅ All manual test sessions completed successfully - DONE (Tasks 7.10-7.14)
5. ✅ Mock verification tests pass against real API - DONE (100% pass, Task 8.1)
6. ✅ Documentation updated - DONE (docs/README.md, Task 8.4)
7. ✅ Final acceptance test passed by human - DONE (Task 8.6)
8. ✅ Basic LLM chat works with streaming responses - DONE (Phase 1.6)
9. ✅ Commands work: /clear, /mark, /rewind, /help, /model, /system - DONE (all 6 commands implemented)
10. ✅ Messages stored in-memory only (database is Phase 2) - DONE

**🎉 PHASE 1 COMPLETE - All Success Criteria Met! 🎉**

## Notes

- Manual verification points are critical - do not skip
- If any test fails, fix before proceeding
- Database persistence deferred to Phase 2 - acceptable to lose messages on exit for Phase 1

