# Task: OpenAI JSONL HTTP Request/Response Logging

## Target
Infrastructure: JSONL logging for OpenAI HTTP traffic

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/openai/client_multi_request.c (request construction)
- src/openai/client_multi_callbacks.c (response handling)
- src/logger.h (JSONL logger API)

### Pre-read Tests (patterns)
- tests/unit/openai/client_multi_test.c

## Pre-conditions
- `make check` passes
- Task `openai-remove-debug-pipe.md` completed
- Debug_pipe removed from OpenAI code
- JSONL logger available

## Task
Add JSONL logging for all OpenAI HTTP requests and responses. Log at application level (not wire level) with structured JSON data.

Log request when constructed:
```json
{"event":"http_request","method":"POST","url":"https://api.openai.com/v1/chat/completions","headers":{"Content-Type":"application/json"},"body":{...}}
```

Log response when complete:
```json
{"event":"http_response","status":200,"body":{...}}
```

Headers: Log all request headers EXCEPT Authorization (omit it entirely).

Location: Add logging in `ik_openai_multi_add_request()` for requests and in completion callback for responses.

## TDD Cycle

### Red
1. Add test in `tests/unit/openai/jsonl_logging_test.c`:
   - Mock file operations to capture log writes
   - Create OpenAI request
   - Verify request log entry written with correct structure
   - Trigger completion callback with response
   - Verify response log entry written with correct structure
   - Verify Authorization header NOT logged
   - Verify body is valid JSON object (not escaped string)
2. Run `make check` - expect failure (no logging yet)

### Green
1. In `src/openai/client_multi_request.c`, add to `ik_openai_multi_add_request()`:
   - After constructing request, before curl_multi_add_handle:
   - Create log doc with `ik_log_create()`
   - Add "event":"http_request"
   - Add "method":"POST"
   - Add "url" with full URL
   - Add "headers" object with Content-Type (skip Authorization)
   - Parse request_body JSON and add as "body" object (not string)
   - Call `ik_log_debug(doc)`
2. In completion callback (where HTTP response is received):
   - Create log doc
   - Add "event":"http_response"
   - Add "status" (HTTP status code from curl)
   - Parse response body JSON and add as "body" object
   - Call `ik_log_debug(doc)`
3. Run `make check` - expect pass

### Refactor
1. Extract JSON parsing logic to helper if repeated
2. Verify no logging on error paths (or use appropriate log level)
3. Ensure memory cleanup (docs freed after logging)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- All HTTP requests logged with structured JSON
- All HTTP responses logged with structured JSON
- Authorization header excluded from logs
- 100% test coverage for logging code
