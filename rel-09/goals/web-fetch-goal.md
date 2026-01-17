## Objective

Implement web-fetch-tool external executable per plan specifications, providing HTTP content fetching with HTML-to-markdown conversion, pagination via offset/limit, and no credential requirements.

## Reference

**Plan Documents:**
- $CDD_DIR/plan/README.md - Overview and key decisions
- $CDD_DIR/plan/tool-schemas.md - Complete JSON schema for web-fetch-tool (lines 342-663)
- $CDD_DIR/plan/tool-implementation.md - Memory management, return conventions, standard tool structure
- $CDD_DIR/plan/build-integration.md - Makefile patterns, dependencies, installation (libxml2 required)

**Research:**
- $CDD_DIR/research/html-to-markdown.md - libxml2 usage, DOM traversal, element mapping, conversion algorithm

**User Stories:**
- $CDD_DIR/user-stories/web-fetch.md - Fetching URLs, HTML conversion, pagination behavior

**Existing Tool Examples:**
- src/tools/bash/main.c - Standard tool structure: --schema, stdin reading, talloc pattern
- src/tools/file_read/main.c - JSON parsing with yyjson, parameter extraction, offset/limit pattern
- src/tools/grep/main.c - Error handling patterns, exit codes
- src/json_allocator.h - yyjson talloc integration
- tests/unit/tools/tool_external_test.c - Integration test pattern

**External Tool Protocol:**
- project/external-tool-architecture.md - Tool discovery, --schema protocol, invocation sequence

## Outcomes

**Implementation:**
- src/tools/web_fetch/main.c created with complete tool implementation
- Tool follows standard structure from tool-implementation.md (--schema, stdin, JSON parsing, operation, output, cleanup)
- Memory management uses talloc with single root context (all allocations off ctx, single talloc_free on all paths)
- Return conventions: exit 0 for successful execution (check JSON success field), exit 1 for crashes only
- Mixed library memory handled correctly: xmlFreeDoc() before talloc_free(), curl_easy_cleanup() before talloc_free()

**--schema Flag:**
- Returns exact JSON schema from tool-schemas.md:350-377
- Includes name: "web_fetch"
- Includes description mentioning HTML→markdown conversion
- Includes all parameters: url (required), offset (optional), limit (optional)
- Exits with code 0

**HTTP Fetching:**
- Makes HTTP GET request to specified URL
- Uses libcurl for HTTP client
- Follows redirects automatically (CURLOPT_FOLLOWLOCATION)
- 10-second timeout (shorter than ikigai's 30-second tool timeout)
- Captures response body in talloc-allocated buffer
- Uses response_buffer struct pattern from tool-implementation.md:348-352
- Returns final URL if redirected (may differ from requested URL)

**HTML Parsing:**
- Uses libxml2 htmlReadMemory() to parse HTML (tool-schemas.md:640)
- Parse options: HTML_PARSE_NOERROR to suppress error messages
- Extract title from <title> tag
- Get root element via xmlDocGetRootElement()
- Handle parsing failures with PARSE_ERROR

**HTML to Markdown Conversion:**
- Implements conversion algorithm from research/html-to-markdown.md
- Recursive DOM tree traversal using node->children and node->next pointers
- Element mapping per tool-schemas.md:646-654:
  - <h1> → "# Heading"
  - <h2> → "## Heading"
  - <p> → Plain text with blank line
  - <a href="url"> → "[text](url)"
  - <strong>, <b> → "**text**"
  - <em>, <i> → "*text*"
  - <code> → "`text`"
  - <ul>, <li> → Markdown lists
- Strip elements: <script>, <style>, <nav>, comments
- Preserve text content from text nodes
- Handle nested elements correctly

**Pagination:**
- offset/limit applied to markdown lines, NOT raw HTML (tool-schemas.md:421-426)
- Count lines in converted markdown output
- If offset specified: skip first N lines
- If limit specified: return at most N lines
- If offset exceeds total lines: return empty content
- If limit exceeds remaining lines: return all remaining
- Similar behavior to file_read tool

**Error Handling:**
- NETWORK_ERROR for connection failures, timeouts, DNS resolution failures
- HTTP_ERROR for HTTP status >= 400 (404, 500, etc.) with status code in message
- PARSE_ERROR for HTML parsing failures
- INVALID_URL for malformed URL strings
- All errors: exit 0 with JSON {success: false, error: "...", error_code: "..."}
- No credential errors (tool requires no credentials)
- No _event field ever emitted (tool-schemas.md:479)

**Output Format:**
- Success: {success: true, url: "...", title: "...", content: "markdown..."}
- url field contains final URL (may differ from requested if redirected)
- title field contains text from <title> tag
- content field contains markdown-converted HTML (after offset/limit applied)
- Error: {success: false, error: "message", error_code: "CODE"}
- All output to stdout
- Never write to stderr (protocol is stdout-only)

**Build Integration:**
- Makefile targets added per build-integration.md:53-58
- Source at src/tools/web_fetch/main.c
- Builds to libexec/ikigai/web-fetch-tool
- Uses $(TOOL_COMMON_SRCS) for shared infrastructure
- Links against $(CLIENT_LIBS), -lcurl, and libxml2
- libxml2 added per-target via $(shell pkg-config --libs libxml-2.0)
- Convenience target: web_fetch_tool
- Added to aggregate tools: target
- Added to install: target (build-integration.md:208)
- Added to uninstall: target (build-integration.md:244)

**Library Memory Management:**
- libxml2 uses xmlMalloc/xmlFree internally
- CRITICAL: Call xmlFreeDoc(doc) before talloc_free(ctx)
- CRITICAL: Call xmlCleanupParser() at end if needed (check libxml2 docs)
- libcurl uses malloc/free internally
- CRITICAL: Call curl_easy_cleanup(curl) before talloc_free(ctx)
- Pattern from tool-implementation.md:58-83

**Testing:**
- Unit tests in tests/unit/tools/web_fetch_test.c
- Test --schema flag returns valid JSON
- Test empty stdin returns exit 1
- Test invalid JSON returns exit 1
- Test malformed URL returns INVALID_URL error
- Test HTTP 404 returns HTTP_ERROR with status code
- Test HTML parsing with various element types (headings, paragraphs, links, lists)
- Test HTML→markdown conversion accuracy using fixtures
- Test offset/limit pagination (offset only, limit only, both, offset exceeds total)
- Test title extraction from <title> tag
- Test script/style stripping
- Test redirect handling (final URL returned)
- Integration test via ik_tool_external_exec() pattern (see tool_external_test.c)
- Memory leak testing with valgrind (no leaks, libxml2 cleanup verified)
- Coverage: 100% of tool code

**HTML→Markdown Test Fixtures:**
- Create test HTML fixtures in tests/fixtures/ or inline in test code
- Example: Simple HTML with headings, paragraphs, links
- Example: Nested lists (ul/li)
- Example: Mixed formatting (bold, italic, code)
- Example: HTML with scripts/styles to strip
- Verify conversion produces expected markdown

## Acceptance

**Build Success:**
- `make web_fetch_tool` completes without errors
- Binary created at libexec/ikigai/web-fetch-tool
- Binary is executable
- libxml2 dependency satisfied (pkg-config --exists libxml-2.0)

**Schema Validation:**
- `libexec/ikigai/web-fetch-tool --schema` outputs valid JSON
- JSON matches schema from tool-schemas.md:350-377 exactly
- Exit code 0

**Basic Execution:**
- Tool can fetch simple URLs and convert to markdown
- No credentials required (works immediately without configuration)
- Returns valid JSON with success, url, title, content fields

**HTML Conversion Accuracy:**
- Headings converted correctly (# for h1, ## for h2, etc.)
- Links converted to markdown format [text](url)
- Bold/italic converted to **text** and *text*
- Paragraphs separated by blank lines
- Scripts and styles stripped from output

**Pagination:**
- offset parameter skips correct number of lines in markdown
- limit parameter returns correct number of lines
- offset + limit work together correctly
- Empty content returned when offset exceeds total lines

**Error Handling:**
- Invalid URLs return INVALID_URL error with exit 0
- HTTP errors (404, 500) return HTTP_ERROR with status code
- Network failures return NETWORK_ERROR
- Malformed HTML parsing failures return PARSE_ERROR

**Memory Safety:**
- `echo '{"url":"https://example.com"}' | valgrind --leak-check=full libexec/ikigai/web-fetch-tool` shows no memory leaks
- "All heap blocks were freed -- no leaks are possible"
- xmlFreeDoc() called for all parsed documents
- curl_easy_cleanup() called for all curl handles
- No mixed library memory leaks (libxml2, libcurl both cleaned up)

**Tests Pass:**
- `make check` passes (all unit tests)
- All web_fetch tests pass
- HTML→markdown conversion tests verify correct output
- Coverage reports 100% for src/tools/web_fetch/main.c

**Code Quality:**
- No compiler warnings with existing warning flags
- No security warnings with existing security flags
- Follows project conventions (naming, formatting, error handling)
- talloc used correctly (all allocations off ctx, all paths call talloc_free)
- Library cleanup happens before talloc_free on all paths (success and error)
