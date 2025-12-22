# Verified - Do Not Re-Check

## File Paths & Structure
- Factory file paths: `provider-factory.md` creates `factory.h`/`factory.c` (not `provider_common.*`)
- Error file paths: `verify-foundation.md` expects `src/providers/common/error.*`
- `order.json` structure valid (no duplicates, files exist)
- All `/load` skill references valid
- All `scratch/plan/*.md` references valid

## Dependency Ordering
- All 64 tasks have correct topological ordering in order.json
- No circular dependencies exist
- All dependency references point to existing tasks
- `verify-infrastructure.md` ordering: position 21, after test tasks
- `verify-providers.md` at position 58
- `verify-foundation.md` depends on all credential test tasks
- Credential task chain correct: credentials-core.md → credentials-migrate.md → credentials-tests-helpers.md

## Type & Function Definitions
- `ik_provider_create` signature: 3-param `(ctx, name, &out)` everywhere
- `ik_provider_completion_t` naming (avoids collision with `ik_http_completion_t`)
- `ik_provider_completion_cb_t` standardized across all files
- HTTP vs provider completion types: HTTP uses `ik_http_completion_cb_t`, providers use `ik_provider_completion_cb_t`
- Test callbacks use `ik_provider_completion_t` (not `ik_http_completion_t`)
- Callback return types: all test callbacks return `res_t` with `OK(NULL)`
- Error enum prefix: `IK_ERR_*` (not `IK_ERR_CAT_*`)
- `ERR_AGENT_NOT_FOUND` in `error_code_str()` switch
- `ik_request_build_from_conversation()` defined in request-builders.md, repl-provider-routing.md depends on it
- Send implementations use async `ik_http_multi_add_request()` pattern (anthropic-response.md, google-response.md)

## Provider Types (provider-types.md defines all)
- `ik_thinking_level_t` enum (IK_THINKING_NONE, IK_THINKING_LOW, IK_THINKING_MED, IK_THINKING_HIGH)
- `ik_finish_reason_t` enum (IK_FINISH_STOP, IK_FINISH_LENGTH, etc.)
- `ik_content_type_t`, `ik_role_t`, `ik_tool_choice_t` enums
- `ik_error_category_t` enum (IK_ERR_AUTH, IK_ERR_RATE_LIMIT, etc.)
- `ik_stream_event_type_t` enum
- `ik_request_t`, `ik_response_t`, `ik_provider_completion_t` structs
- Provider vtable with fdset, perform, timeout, info_read, start_request, start_stream

## Credentials (credentials-core.md defines all)
- `ik_credentials_load()`, `ik_credentials_get()`, `ik_credentials_t`
- Environment variables: OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY
- `verify-mocks-providers.md` credential precondition: uses `make verify-credentials`

## Request/Response Builders (request-builders.md defines all)
- `ik_request_create()`, `ik_request_set_system()`, `ik_request_add_message()`
- `ik_request_add_message_blocks()`, `ik_request_set_thinking()`, `ik_request_add_tool()`
- `ik_response_create()`, `ik_response_add_content()`
- `ik_content_block_text()`, `ik_content_block_tool_call()`, `ik_content_block_tool_result()`, `ik_content_block_thinking()`

## Error Utilities (error-core.md defines all)
- `ik_error_category_name()`, `ik_error_is_retryable()`
- `ik_error_user_message()`, `ik_error_calc_retry_delay_ms()`

## SSE Parser (sse-parser.md defines all)
- `ik_sse_parser_t`, `ik_sse_parser_create()`, `ik_sse_parser_feed()`
- `ik_sse_parser_next()`, `ik_sse_event_is_done()`

## Mock Infrastructure (tests-mock-infrastructure.md)
- Complete implementation code provided (lines 83-311)
- `mock_curl_multi_reset()`, `mock_curl_multi_set_response()`, `mock_curl_multi_set_streaming_response()`
- `mock_curl_multi_set_error()`, `mock_curl_multi_set_fd()`, `mock_curl_multi_load_cassette()`
- MOCKABLE overrides for curl_multi_fdset_, curl_multi_perform_, curl_multi_info_read_, etc.

## Provider-Specific Functions
- Anthropic: `ik_anthropic_create()`, `ik_anthropic_thinking_budget()`, `ik_anthropic_supports_thinking()`, `ik_anthropic_serialize_request()`, `ik_anthropic_build_headers()`, `ik_anthropic_handle_error()`
- Google: `ik_google_create()`, `ik_google_model_series()`, `ik_google_thinking_budget()`, `ik_google_thinking_level_str()`
- OpenAI: `ik_openai_create()`, `ik_openai_reasoning_effort()`, `ik_openai_is_reasoning_model()`, `ik_openai_supports_temperature()`
