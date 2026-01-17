## Objective

Implement web-search-brave-tool external executable per plan specifications, providing Brave Search API integration with complete credential discovery, domain filtering, pagination, and config_required event emission.

## Reference

**Plan Documents:**
- $CDD_DIR/plan/README.md - Overview and key decisions
- $CDD_DIR/plan/tool-schemas.md - Complete JSON schema for web-search-brave-tool (lines 10-180)
- $CDD_DIR/plan/tool-implementation.md - Memory management, return conventions, standard tool structure
- $CDD_DIR/plan/build-integration.md - Makefile patterns, dependencies, installation

**Research:**
- $CDD_DIR/research/brave.md - Brave Search API specification, endpoints, parameters, response format
- $CDD_DIR/research/all-providers-comparison.md - Provider selection rationale

**User Stories:**
- $CDD_DIR/user-stories/first-time-discovery.md - Tool behavior without credentials, config_required event
- $CDD_DIR/user-stories/successful-search.md - Normal operation with valid credentials
- $CDD_DIR/user-stories/rate-limit-exceeded.md - Rate limit error handling

**Existing Tool Examples:**
- src/tools/bash/main.c - Standard tool structure: --schema, stdin reading, talloc pattern
- src/tools/file_read/main.c - JSON parsing with yyjson, parameter extraction
- src/tools/grep/main.c - Error handling patterns, exit codes
- src/json_allocator.h - yyjson talloc integration
- tests/unit/tools/tool_external_test.c - Integration test pattern

**External Tool Protocol:**
- project/external-tool-architecture.md - Tool discovery, --schema protocol, invocation sequence

## Outcomes

**Implementation:**
- src/tools/web_search_brave/main.c created with complete tool implementation
- Tool follows standard structure from tool-implementation.md (--schema, stdin, JSON parsing, operation, output, cleanup)
- Memory management uses talloc with single root context (all allocations off ctx, single talloc_free on all paths)
- Return conventions: exit 0 for successful execution (check JSON success field), exit 1 for crashes only

**--schema Flag:**
- Returns exact JSON schema from tool-schemas.md:16-61
- Includes name: "web_search_brave"
- Includes description mentioning Brave Search API
- Includes all parameters: query (required), count, offset, allowed_domains, blocked_domains
- Exits with code 0

**Credential Discovery:**
- Checks environment variable BRAVE_API_KEY first (highest precedence)
- Falls back to ~/.config/ikigai/credentials.json at web_search.brave.api_key
- Returns auth error with config_required event if credentials missing
- Uses yyjson to parse credentials.json

**Brave API Integration:**
- Makes HTTP GET request to Brave Search API endpoint
- Uses libcurl for HTTP client
- Sets X-Subscription-Token header with API key
- Passes query as q parameter, count as count parameter, offset as offset parameter
- Parses JSON response using yyjson
- Maps Brave response format to common format per tool-schemas.md:114-119

**Domain Filtering:**
- Implements post-processing algorithm from tool-schemas.md:542-574
- Exact hostname match, case-insensitive
- allowed_domains: whitelist (empty = include all)
- blocked_domains: blacklist (applied after allowed_domains)
- URL deduplication during filtering

**Error Handling:**
- AUTH_MISSING error with _event field when credentials not found (tool-schemas.md:123-140)
- RATE_LIMIT error for HTTP 429 responses
- NETWORK_ERROR for connection failures, timeouts
- API_ERROR for other API failures
- All errors: exit 0 with JSON {success: false, error: "...", error_code: "..."}

**Output Format:**
- Success: {success: true, results: [{title, url, snippet}...], count: N}
- Error: {success: false, error: "message", error_code: "CODE", _event: {...}} (if auth error)
- All output to stdout
- Never write to stderr (protocol is stdout-only)

**Build Integration:**
- Makefile targets added per build-integration.md:33-50
- Source at src/tools/web_search_brave/main.c
- Builds to libexec/ikigai/web-search-brave-tool
- Uses $(TOOL_COMMON_SRCS) for shared infrastructure
- Links against $(CLIENT_LIBS) and -lcurl
- Convenience target: web_search_brave_tool
- Added to aggregate tools: target
- Added to install: target (build-integration.md:206)
- Added to uninstall: target (build-integration.md:242)

**Testing:**
- Unit tests in tests/unit/tools/web_search_brave_test.c
- Test --schema flag returns valid JSON
- Test empty stdin returns exit 1
- Test invalid JSON returns exit 1
- Test missing credentials returns exit 0 with auth error and _event field
- Test malformed API response handling
- Integration test via ik_tool_external_exec() pattern (see tool_external_test.c)
- Memory leak testing with valgrind (no leaks expected)
- Coverage: 100% of tool code

## Acceptance

**Build Success:**
- `make web_search_brave_tool` completes without errors
- Binary created at libexec/ikigai/web-search-brave-tool
- Binary is executable

**Schema Validation:**
- `libexec/ikigai/web-search-brave-tool --schema` outputs valid JSON
- JSON matches schema from tool-schemas.md:16-61 exactly
- Exit code 0

**Execution Without Credentials:**
- `echo '{"query":"test"}' | libexec/ikigai/web-search-brave-tool` (no BRAVE_API_KEY, no credentials.json)
- Returns exit 0
- stdout contains JSON with success: false
- JSON includes error_code: "AUTH_MISSING"
- JSON includes _event field with kind: "config_required"
- _event.content includes setup instructions
- _event.data includes tool: "web_search_brave", credential: "api_key", signup_url

**Memory Safety:**
- `echo '{"query":"test"}' | valgrind --leak-check=full libexec/ikigai/web-search-brave-tool` shows no memory leaks
- "All heap blocks were freed -- no leaks are possible"

**Tests Pass:**
- `make check` passes (all unit tests)
- All web_search_brave tests pass
- Coverage reports 100% for src/tools/web_search_brave/main.c

**Code Quality:**
- No compiler warnings with existing warning flags
- No security warnings with existing security flags
- Follows project conventions (naming, formatting, error handling)
- talloc used correctly (all allocations off ctx, all paths call talloc_free)
