# rel-09 Plan Verification Gaps

**Verification Date:** 2026-01-16

This document identifies gaps, contradictions, and missing specifications in the rel-09 plan that must be resolved before implementation.

**Status:** 7 of 9 gaps resolved (see verified.md for details)

---

## Critical Gap (Blocks Implementation)

### ✅ RESOLVED: stderr Protocol Conflict with rel-08

**Resolution:** See verified.md (2026-01-16 - stderr Protocol Conflict Resolved)

**Original Issue:** rel-09 plan requires event mechanism that doesn't exist in rel-08

**Issue:** rel-09 plan requires event mechanism that doesn't exist in rel-08

**Current Protocol** (src/repl_tool.c:50-58, src/tool_wrapper.c):
- Tools write JSON to stdout
- `tool_external_exec()` captures stdout
- ikigai wraps tool output:
  - Exit 0: `{"tool_success": true, "result": {...tool output...}}`
  - Exit non-zero: `{"tool_success": false, "error": "...", "error_code": "..."}`
- stderr redirected to /dev/null (debugging messages discarded)
- All communication via stdout + exit codes

**rel-09 Requirement** (architecture-separate-tools.md:137, tool-schemas.md:158-172):
- Tools need to emit config_required events that:
  - Are stored in database (messages table)
  - Are shown to user (dim yellow warning)
  - Are NOT sent to LLM (not included in tool_result)
- Plan proposes: write events to stderr

**Problem:** No mechanism exists for out-of-band metadata (events separate from tool results)

**Resolution Options:**

**Option 1: Use JSON output field (RECOMMENDED)**
- Tools include optional `_event` field in JSON output:
  ```json
  {
    "success": false,
    "error": "Web search requires API key...",
    "error_code": "AUTH_MISSING",
    "_event": {
      "kind": "config_required",
      "content": "⚠ Configuration Required\n\n...",
      "data": {"tool": "web_search_brave", "credential": "api_key", ...}
    }
  }
  ```
- `ik_tool_wrap_success()` extracts `_event` field, stores in database, removes from result
- Wrapped output to LLM: `{"tool_success": true, "result": {"success": false, "error": "..."}}`
- Pros: No breaking changes, works with existing infrastructure, clean separation
- Cons: Slightly more complex JSON parsing in wrapper
- Implementation: Modify tool_wrapper.c to extract/handle `_event` field

**Option 2: Capture stderr**
- Update tool_external.c to capture stderr instead of discarding it
- Parse JSON events from stderr, store in database
- Pros: Clean two-channel pattern, aligns with plan's intent
- Cons: Breaking change (need to audit all existing tools' stderr output)
- Implementation: Add stderr pipe to tool_external.c:55-60

**Option 3: Separate event file**
- Tools write events to /tmp/ikigai-events-{pid}.json
- Pros: No breaking changes
- Cons: Filesystem coupling, cleanup complexity, race conditions

**Recommendation:** **Option 1** (JSON `_event` field) because:
1. No breaking changes - existing tools unaffected
2. Works within current protocol
3. Event extracted before LLM sees it
4. Simpler implementation than stderr capture
5. Prefix `_` indicates internal/metadata field

**Update required:** Plan should specify Option 1 instead of stderr approach

---

## Major Gaps (Block Implementation)

### 1. Missing Internal Specifications

**Issue:** Plan lacks coordination-layer specifications

plan/README.md:119-123 promises "Module names, Major structs (names and purpose), Key function signatures" but none of the plan documents deliver this.

**What's missing:**
- Module names for C implementations (ik_web_search_brave? ik_web_fetch?)
- Struct definitions (even just names and purposes)
- Function signatures for parsing/execution/response generation
- Specification of --schema flag implementation pattern
- Specification of stdin/stdout handling in C
- Memory management patterns and ownership rules
- Error handling patterns (res_t with OK()/ERR()?)

**Impact:** External tools are separate executables, but they still need internal coordination specs:
- How to structure C code
- What functions/structs to create
- Whether to share code between tools (common HTTP client? JSON helpers?)
- Memory management approach

**Current plan specifies:**
- External JSON protocol ✓
- API integration requirements ✓
- Build dependencies ✓

**Plan should also specify:**
- Internal module structure
- Key function signatures
- Struct definitions for tool state
- Shared utility modules (if any)

**Recommendation:** Add new plan document `tool-implementation.md` specifying:
- Common struct pattern: `struct tool_context { void *talloc_ctx; ... }`
- Function signatures: `res_t parse_args(...)`, `res_t execute(...)`, `res_t format_response(...)`
- Memory management: All tools use talloc, free on error paths
- Error handling: Internal functions return res_t, map to JSON at boundary

---

### ✅ RESOLVED: 2. Memory Management Not Specified

**Resolution:** See verified.md (2026-01-16 - Memory Management and Return Value Conventions Documented)

**Original Issue:** Plan doesn't specify memory management approach for external tools

**Questions:**
- Do tools use talloc (ikigai's approach) or malloc/free?
- Do tools link against ikigai's talloc, or are they standalone?
- What are ownership rules for allocated memory?
- How is memory cleaned up on error paths?
- How do tools handle libxml2/libcurl allocated memory?

**Context from existing tools** (src/tools/bash/main.c, src/tools/file_read/main.c):
- All existing tools use talloc
- Pattern: `void *ctx = talloc_new(NULL);` at start
- All allocations off this context
- Single `talloc_free(ctx);` cleans up everything

**Memory allocation needed in web tools:**
- JSON parsing (stdin input, API responses, stdout output)
- HTTP response bodies (can be large)
- HTML DOM trees (libxml2 - xmlDocPtr, xmlNodePtr)
- Markdown conversion buffers
- Credential file parsing

**Mixed library concern:**
- libxml2 uses its own allocator (xmlMalloc/xmlFree)
- libcurl uses malloc/free internally
- Need to specify ownership transfer at library boundaries

**Impact:** Risk of memory leaks or double-frees without clear ownership rules.

**Recommendation:** Specify that tools use talloc pattern from existing tools, with explicit guidance on:
- Main context: `void *ctx = talloc_new(NULL);`
- All tool allocations: talloc_array(), talloc_strdup() off ctx
- Library allocations: Must be freed with library-specific functions before talloc_free(ctx)
- Error paths: Just talloc_free(ctx); return non-zero

---

### ✅ RESOLVED: 3. Return Value Convention Unclear

**Resolution:** See verified.md (2026-01-16 - Memory Management and Return Value Conventions Documented)

**Original Issue:** Plan doesn't specify internal error handling pattern for external tools

**Question:** Do external tools use:
- ikigai's res_t with OK()/ERR() patterns internally?
- Standard C int return codes (0 = success)?
- Some other pattern?

**What needs specification:**
1. Internal function return conventions
2. How internal errors map to JSON error responses
3. How internal errors map to exit codes
4. Whether external tools link against ikigai's error.h

**Context from existing tools:**
- Tools don't use res_t currently
- Tools use simple error handling: print to stderr, return 1
- Tools don't have complex internal call chains

**Web tools will have more complex error handling:**
- HTTP errors (timeout, connection refused, DNS failure)
- API errors (401 unauthorized, 429 rate limit, 500 server error)
- JSON parsing errors (malformed response, missing fields)
- Credential errors (missing, invalid format)

**Impact:** Without specification, implementers must guess whether to:
- Use ikigai patterns (requires linking against ikigai error.c)
- Use standard C patterns (standalone executables)
- Reinvent similar patterns

**Recommendation:** Specify simple error handling pattern:
- Internal functions return int (0 = success, -1 = error)
- Error messages accumulated in error buffer (char *error)
- At top level: convert error to JSON response, return 1
- No res_t needed for external tools (simpler, standalone)

---

### ✅ RESOLVED: 4. HTTP Client Library Not Decided

**Resolution:** See verified.md (2026-01-16 - HTTP Client Library Decided)

**Original Issue:** build-integration.md:65-82 lists HTTP library options but doesn't make final decision

**Options listed:**
1. libcurl (recommended but not decided)
2. http-parser + raw sockets
3. neon

**Says:** "Decision needed: Which HTTP client library to use?"
**Recommends:** "libcurl for reliability and TLS handling"

**Impact:** Can't implement tools without knowing:
- Which headers to include
- Which API functions to call
- How to configure timeouts
- How to handle TLS
- Error code mapping

**libcurl rationale:**
- De facto standard for HTTP in C
- Handles HTTPS, redirects, timeouts automatically
- Available on all platforms
- Well-documented, battle-tested

**Recommendation:** Commit to libcurl and update build-integration.md from "Decision needed" to "Decision: libcurl selected for..."

---

### 5. Test Strategy Missing

**Issue:** Plan doesn't specify test strategy for external tools

**Questions:**
1. Are tools covered by ikigai's test suite? If so, how?
2. Do tools need independent test suites?
3. How to test without making real API calls? (mocking? test doubles?)
4. How to test credential discovery? (temporary config files?)
5. How to test error conditions? (mock HTTP failures, rate limits, timeouts)
6. How to test HTML→markdown conversion? (fixtures?)
7. Integration tests between ikigai and tools?

**Context:**
- Project requires 100% test coverage
- Tools make HTTP API calls (Brave, Google)
- Tools read filesystem (credentials.json)
- Tools have complex logic:
  - JSON parsing (stdin, API responses, credentials)
  - HTTP client code
  - HTML→markdown conversion
  - Credential precedence
  - Error mapping

**Impact:** Without test strategy, 100% coverage requirement can't be met or verified.

**Recommendation:** Add test strategy section specifying:
- Unit tests for each tool (tests/unit/tools/web-*.c)
- Mock HTTP responses (fixture JSON files)
- Mock credential files (temporary test configs)
- Integration tests via ikigai (spawn tool, verify output)
- HTML→markdown fixtures (input.html → expected.md)
- Coverage target: 100% of tool code

---

## Medium Gaps (Incomplete Specifications)

### ✅ RESOLVED: 6. Error Handling Protocol Contradiction

**Resolution:** Resolved as part of stderr protocol fix (see verified.md)

**Original Issue:** Contradiction in stderr usage within plan documents - plan documents disagreed on whether to use stderr for events.

**Resolution:** Updated all plan documents to use `_event` field approach instead of stderr. Contradiction eliminated.

---

### 7. External Tool Framework Integration Incomplete

**Issue:** Plan references rel-08's external tool framework but doesn't specify integration details

**What needs specification:**
1. Tool discovery mechanism (how ikigai finds tools in libexec/ikigai/)
2. --schema flag protocol (output format, exit code)
3. Tool invocation sequence (fork/exec? stdin pipe?)
4. stderr event capture (format? line-delimited JSON?)
5. Timeout enforcement (SIGTERM? SIGKILL?)
6. Exit code interpretation (0 = success, non-zero = error?)

**Current plan mentions:**
- "Standard protocol: implements --schema and JSON stdin/stdout" ✓
- "ikigai captures stderr and emits database event" (how?)
- "30-second timeout enforced by ikigai" (how?)

**Known from source code:**
- Tool discovery scans libexec/ikigai/, ~/.ikigai/tools/, .ikigai/tools/
- Tools invoked via fork/exec with stdin/stdout pipes
- 30-second timeout via alarm() syscall
- Non-zero exit = tool failure

**Missing from plan:**
- Exact invocation contract
- Event format on stderr (JSON? line-delimited? one event per line?)
- Error handling between ikigai and tool

**Recommendation:** Either:
1. Add section to plan specifying complete protocol
2. Reference project/external-tool-architecture.md with note about rel-09 extensions (stderr capture)

---

### ✅ RESOLVED: 8. Domain Filtering Implementation Incomplete

**Resolution:** See verified.md (2026-01-16 - Domain Filtering Algorithm Specified)

**Original Issue:** Domain filtering behavior not fully specified

**Brave Search:**
- tool-schemas.md:86 says "implemented via post-processing"
- Doesn't specify the algorithm (string matching? exact domain? subdomain handling?)

**Google Search:**
- tool-schemas.md:257 says "only supports single domain" for blocked_domains
- Doesn't specify behavior with multiple blocked_domains (error? use first? ignore rest?)

**What needs specification:**
1. **Brave post-processing algorithm:**
   - Match full domain only or include subdomains?
   - Match "example.com" against "www.example.com"? "blog.example.com"?
   - Case sensitivity?
   - Match against URL's domain after normalization?

2. **Google multi-domain handling:**
   - How to handle allowed_domains array? (multiple calls? OR operator? only first?)
   - How to handle blocked_domains array? (error? first only? ignore rest?)

3. **Domain validation:**
   - Should tools validate domain format?
   - What's a valid domain string? (TLD required? wildcards allowed?)

**Impact:** Inconsistent filtering behavior between providers without specification.

**Recommendation:** Specify exact filtering algorithm OR declare "best effort" with undefined edge cases. For example:
- Brave: Exact domain match, case-insensitive, no subdomain wildcarding
- Google: First allowed_domain only, first blocked_domain only, document limitation

---

### ✅ RESOLVED: 9. Database Integration Incomplete

**Resolution:** See verified.md (2026-01-16 - Database Integration Specified)

**Original Issue:** Plan doesn't specify how to store config_required events in database

**Finding:** The `messages` table already exists with support for arbitrary event types

**Evidence** (share/ikigai/migrations/001-initial-schema.sql):
- `messages.kind` TEXT field (event type discriminator)
- `messages.content` TEXT field
- `messages.data` JSONB field (event-specific metadata)
- Event kinds currently: clear, system, user, assistant, mark, rewind

**Implication:** config_required events can use existing messages table with:
- `kind = 'config_required'`
- `content` = formatted warning text with setup instructions
- `data` = JSON object: `{"tool": "web_search_brave", "credential": "api_key", "signup_url": "..."}`

**Missing from plan:**
1. Specification that config_required uses existing messages table
2. Schema for data JSONB field
3. Integration point: Where/when ikigai inserts config_required message
4. Display logic: How scrollback queries and renders config_required messages
5. Association with tool call (same turn? separate message?)

**Recommendation:** Add section specifying:
- Event storage: Use messages table with kind='config_required'
- Event structure: content (user display), data (structured metadata)
- Integration: After tool execution, if stderr contains JSON event, insert to messages
- Display: Scrollback queries messages, renders config_required in dim yellow

---

## Summary

**Total gaps identified:** 9 (1 critical, 5 major, 3 medium)
**Resolved:** 7 gaps (see verified.md)
**Remaining:** 2 gaps

**✅ Resolved:**
- Critical: stderr protocol conflict → Used `_event` field in JSON
- Major #2: Memory management unspecified → Documented talloc patterns in tool-implementation.md
- Major #3: Return value conventions unclear → Documented exit codes and internal patterns
- Major #4: HTTP library not decided → Committed to libcurl
- Medium #6: Error handling protocol contradiction → Resolved with stderr fix
- Medium #8: Domain filtering incomplete → Specified algorithms in tool-schemas.md
- Medium #9: Database integration incomplete → Specified integration in build-integration.md

**Remaining gaps:**
- Major #1: Missing internal specifications (partially addressed by tool-implementation.md)
- Major #5: Test strategy missing (partially addressed, needs HTTP mocking details)
- Medium #7: External tool framework integration incomplete

**Next steps:**
1. ✅ Resolve critical stderr conflict (DONE - used _event field)
2. ✅ Decide HTTP library (DONE - libcurl)
3. ✅ Document memory management pattern (DONE - tool-implementation.md)
4. ✅ Document return value conventions (DONE - tool-implementation.md)
5. ✅ Specify domain filtering (DONE - tool-schemas.md)
6. ✅ Specify database integration (DONE - build-integration.md)
7. Review remaining gaps (#1, #5, #7) - assess if blocking or acceptable
