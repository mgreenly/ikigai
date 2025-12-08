# Task: Remove debug_pipe Usage from OpenAI Code

## Target
Infrastructure: Remove debug_pipe from OpenAI HTTP layer

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/openai/client_multi_request.c (curl debug output setup)
- src/openai/http_handler.c (HTTP handler)
- src/debug_pipe.h (debug pipe interface)

### Pre-read Tests (patterns)
- tests/unit/openai/curl_debug_callback_test.c
- tests/unit/openai/curl_debug_coverage_complete_test.c
- tests/unit/openai/curl_debug_data_in_only_test.c

## Pre-conditions
- `make check` passes
- Task `repl-logger-init.md` completed
- Logger is integrated with REPL
- OpenAI code uses debug_pipe for curl output

## Task
Remove all debug_pipe usage from OpenAI HTTP code. This includes:
1. Remove `debug_output` parameter from `ik_openai_multi_add_request()`
2. Remove curl debug callback setup (CURLOPT_VERBOSE, CURLOPT_DEBUGFUNCTION, CURLOPT_DEBUGDATA)
3. Remove `ik_openai_curl_debug_output()` function
4. Delete curl debug tests (they test removed functionality)

The debug_pipe infrastructure (src/debug_pipe.c/h) will remain for future use, but no code should call it.

## TDD Cycle

### Red
1. Update `src/openai/client_multi_internal.h`:
   - Remove `FILE *debug_output` parameter from `ik_openai_multi_add_request()`
2. Update all call sites to remove debug_output argument
3. Run `make check` - expect compilation errors and test failures

### Green
1. Remove from `src/openai/client_multi_request.c`:
   - Delete `ik_openai_curl_debug_output()` function
   - Remove debug_output parameter from function signature
   - Remove curl debug option setup (lines 177-182)
2. Update call sites in:
   - `src/openai/client_multi.c`
   - `src/repl_tool.c` (remove debug_pipe passing)
3. Delete test files:
   - `tests/unit/openai/curl_debug_callback_test.c`
   - `tests/unit/openai/curl_debug_coverage_complete_test.c`
   - `tests/unit/openai/curl_debug_data_in_only_test.c`
4. Update Makefile to remove deleted test targets
5. Run `make check` - expect pass

### Refactor
1. Verify no debug_pipe references remain in openai/ directory
2. Confirm debug_pipe.c/h still exist (not deleted)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- No debug_pipe usage in OpenAI code
- Debug curl tests deleted
- debug_pipe infrastructure unchanged (available for future use)
