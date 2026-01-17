## Objective

Implement web-search-google-tool external executable per plan specifications, providing Google Custom Search API integration with complete credential discovery, domain filtering via parallel API calls, pagination, and config_required event emission.

## Reference

**Plan Documents:**
- $CDD_DIR/plan/README.md - Overview and key decisions
- $CDD_DIR/plan/tool-schemas.md - Complete JSON schema for web-search-google-tool (lines 182-340)
- $CDD_DIR/plan/tool-implementation.md - Memory management, return conventions, standard tool structure
- $CDD_DIR/plan/build-integration.md - Makefile patterns, dependencies, installation

**Research:**
- $CDD_DIR/research/google.md - Google Custom Search API specification, endpoints, parameters, response format
- $CDD_DIR/research/all-providers-comparison.md - Provider selection rationale

**User Stories:**
- $CDD_DIR/user-stories/first-time-discovery.md - Tool behavior without credentials, config_required event
- $CDD_DIR/user-stories/google-search.md - Google-specific search behavior
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
- src/tools/web_search_google/main.c created with complete tool implementation
- Tool follows standard structure from tool-implementation.md (--schema, stdin, JSON parsing, operation, output, cleanup)
- Memory management uses talloc with single root context (all allocations off ctx, single talloc_free on all paths)
- Return conventions: exit 0 for successful execution (check JSON success field), exit 1 for crashes only

**--schema Flag:**
- Returns exact JSON schema from tool-schemas.md:190-234
- Includes name: "web_search_google"
- Includes description mentioning Google Custom Search API
- Includes all parameters: query (required), num, start, allowed_domains, blocked_domains
- Exits with code 0

**Credential Discovery:**
- Checks environment variables first (highest precedence):
  - GOOGLE_SEARCH_API_KEY for API key
  - GOOGLE_SEARCH_ENGINE_ID for search engine ID
- Falls back to ~/.config/ikigai/credentials.json at web_search.google.api_key and web_search.google.engine_id
- Returns auth error with config_required event if either credential missing
- Uses yyjson to parse credentials.json
- Event includes both credentials needed (tool-schemas.md:299-314)

**Google API Integration:**
- Makes HTTP GET request(s) to Google Custom Search API endpoint
- Uses libcurl for HTTP client
- Passes query as q parameter, num as num parameter, start as start parameter
- Includes key (API key) and cx (engine ID) in request parameters
- Parses JSON response using yyjson
- Maps Google response format to common format per tool-schemas.md:290-295

**Domain Filtering - Multiple Allowed Domains:**
- Implements parallel API call algorithm from tool-schemas.md:575-634
- When allowed_domains has N domains, makes N parallel API calls
- Splits num parameter across domains using integer division:
  - per_domain = num / domain_count
  - remainder = num % domain_count
  - First remainder domains get (per_domain + 1) results
  - Remaining domains get per_domain results
- Each API call uses siteSearch=domain and siteSearchFilter=i (include)
- Skips API calls where num_for_domain = 0
- Round-robin merge results from all successful calls
- URL deduplication during merge (skip duplicate URLs)
- Handles partial failures (return results from successful calls)

**Domain Filtering - Single Domain:**
- Single allowed_domain: Use native siteSearch with siteSearchFilter=i
- Single blocked_domain: Use native siteSearch with siteSearchFilter=e
- Multiple blocked_domains: Post-processing (same as Brave - exact match, case-insensitive)

**Error Handling:**
- AUTH_MISSING error with _event field when credentials not found (tool-schemas.md:299-314)
- RATE_LIMIT error for HTTP 403 with dailyLimitExceeded (100/day quota)
- NETWORK_ERROR for connection failures, timeouts
- API_ERROR for other API failures
- All errors: exit 0 with JSON {success: false, error: "...", error_code: "..."}

**Output Format:**
- Success: {success: true, results: [{title, url, snippet}...], count: N}
- Error: {success: false, error: "message", error_code: "CODE", _event: {...}} (if auth error)
- All output to stdout
- Never write to stderr (protocol is stdout-only)
- Output format identical to Brave (tool-schemas.md:265-280) for LLM interchangeability

**Build Integration:**
- Makefile targets added per build-integration.md:46-50
- Source at src/tools/web_search_google/main.c
- Builds to libexec/ikigai/web-search-google-tool
- Uses $(TOOL_COMMON_SRCS) for shared infrastructure
- Links against $(CLIENT_LIBS) and -lcurl
- Convenience target: web_search_google_tool
- Added to aggregate tools: target
- Added to install: target (build-integration.md:207)
- Added to uninstall: target (build-integration.md:243)

**Testing:**
- Unit tests in tests/unit/tools/web_search_google_test.c
- Test --schema flag returns valid JSON
- Test empty stdin returns exit 1
- Test invalid JSON returns exit 1
- Test missing credentials (api_key only, engine_id only, both) returns exit 0 with auth error and _event field
- Test single allowed_domain uses native siteSearch
- Test multiple allowed_domains triggers parallel API calls with correct num distribution
- Test round-robin merge with deduplication
- Test partial failure handling (some API calls fail)
- Test malformed API response handling
- Integration test via ik_tool_external_exec() pattern (see tool_external_test.c)
- Memory leak testing with valgrind (no leaks expected)
- Coverage: 100% of tool code

## Acceptance

**Build Success:**
- `make web_search_google_tool` completes without errors
- Binary created at libexec/ikigai/web-search-google-tool
- Binary is executable

**Schema Validation:**
- `libexec/ikigai/web-search-google-tool --schema` outputs valid JSON
- JSON matches schema from tool-schemas.md:190-234 exactly
- Exit code 0

**Execution Without Credentials:**
- `echo '{"query":"test"}' | libexec/ikigai/web-search-google-tool` (no GOOGLE_SEARCH_* env vars, no credentials.json)
- Returns exit 0
- stdout contains JSON with success: false
- JSON includes error_code: "AUTH_MISSING"
- JSON includes _event field with kind: "config_required"
- _event.content includes setup instructions for both api_key and engine_id
- _event.data includes tool: "web_search_google", credentials: ["api_key", "engine_id"]

**Schema Consistency:**
- Schema is byte-for-byte identical to Brave except:
  - name field: "web_search_google" (not "web_search_brave")
  - description field: mentions "Google Custom Search API"
  - num/start parameters instead of count/offset
- This ensures LLM can use either tool interchangeably

**Memory Safety:**
- `echo '{"query":"test"}' | valgrind --leak-check=full libexec/ikigai/web-search-google-tool` shows no memory leaks
- "All heap blocks were freed -- no leaks are possible"
- Parallel API calls don't leak memory (all curl handles cleaned up)

**Tests Pass:**
- `make check` passes (all unit tests)
- All web_search_google tests pass
- Coverage reports 100% for src/tools/web_search_google/main.c

**Code Quality:**
- No compiler warnings with existing warning flags
- No security warnings with existing security flags
- Follows project conventions (naming, formatting, error handling)
- talloc used correctly (all allocations off ctx, all paths call talloc_free)
- Parallel API call implementation handles cleanup on all paths (success, partial failure, total failure)
