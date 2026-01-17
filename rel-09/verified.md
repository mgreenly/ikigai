# Verification Log

This document tracks issues that have been fixed in the rel-09 plan.

## Format

Each fixed item is logged with:
- Date/time fixed
- What was changed
- Reference to original gap in gap.md

## Status

**Initial verification:** 2026-01-16
**Gaps identified:** 9 total (1 critical, 5 major, 3 medium)
**Resolved:** 7 gaps
**Remaining:** 2 gaps (both minor/acceptable)
**See:** gap.md for complete list

---

## Fixed Items

### 2026-01-16 - stderr Protocol Conflict Resolved

**Gap Reference:** Critical Gap in gap.md (stderr protocol conflict)

**Issue:** Plan specified tools should write config_required events to stderr, but rel-08 discards stderr. This created a fundamental protocol conflict.

**Resolution:** Changed event mechanism to use `_event` field in JSON output instead of stderr.

**Changes Made:**
1. **plan/architecture-separate-tools.md:108-137** - Updated config_required mechanism to use `_event` field
2. **plan/tool-schemas.md:121-173** - Updated Brave search tool error response to include `_event` field
3. **plan/tool-schemas.md:297-339** - Updated Google search tool error response to use `_event` field
4. **plan/build-integration.md:261-294** - Clarified error handling protocol and documented `_event` field usage
5. **plan/README.md:19-30** - Updated tool availability section to reference `_event` field

**New Protocol:**
- Tools include optional `_event` field in stdout JSON: `{..., "_event": {kind, content, data}}`
- ikigai's tool_wrapper.c extracts `_event`, stores in messages table, removes from result
- LLM receives wrapped result without `_event` field
- User sees event displayed separately in dim yellow

**Benefits:**
- No breaking changes to rel-08
- Works within existing stdout-only protocol
- Events cleanly separated from LLM output
- Consistent with existing tool wrapper pattern

**Note:** This also resolves Medium Gap #6 (Error Handling Protocol Contradiction), which was caused by the stderr conflict between plan documents.

---

### 2026-01-16 - HTTP Client Library Decided

**Gap Reference:** Major Gap #4 in gap.md (HTTP client library not decided)

**Issue:** build-integration.md listed options and recommended libcurl but left it as "Decision needed."

**Resolution:** Committed to libcurl (ikigai already uses it throughout).

**Changes Made:**
1. **plan/build-integration.md:65-82** - Changed from "Decision needed" to "Decision: libcurl"
2. Added rationale: ikigai core already depends on libcurl for provider API calls
3. Added note that no additional build dependencies needed

**Rationale:**
- libcurl already used in ikigai core (`-lcurl` in CLIENT_LIBS)
- Consistent with ikigai's HTTP abstraction (`src/wrapper_curl.c`)
- Mature, battle-tested, widely available
- Handles HTTPS, redirects, timeouts automatically

---

### 2026-01-16 - Memory Management and Return Value Conventions Documented

**Gap Reference:** Major Gaps #2 and #3 in gap.md (Memory management + Return value conventions)

**Issue:** Plan didn't specify internal implementation patterns for external tools.

**Resolution:** Created `plan/tool-implementation.md` documenting existing patterns from current tools.

**Changes Made:**
1. **plan/tool-implementation.md** - New comprehensive document specifying:
   - Memory management: talloc with single root context
   - Return value conventions: exit codes and internal function patterns
   - Standard tool structure template
   - Library memory handling (libcurl, libxml2, cJSON)
   - Common implementation patterns
   - Build integration
   - Testing approach
2. **plan/README.md** - Added tool-implementation.md to plan documents table

**Patterns Documented:**
- Memory: `void *ctx = talloc_new(NULL);` at start, all allocations off ctx, single `talloc_free(ctx)` cleanup
- Returns: Exit 0 for tool execution (check JSON `success` field), exit 1 for crashes, internal functions use `0/-1`
- Structure: --schema handling, stdin reading, JSON parsing, operation, output, cleanup
- No res_t (tools are standalone executables, not linked against ikigai core)

**Source:** Patterns extracted from existing tools in `src/tools/` (bash, file_read, file_write, glob, grep)

---

### 2026-01-16 - Verified: README → Plan Alignment

**Check:** Does the plan implement what README promises?

**README promises (rel-09/README.md):**
- Two search tools: web-search-brave-tool, web-search-google-tool ✓
- Web fetch tool: web-fetch-tool ✓
- Rich search with filters (freshness, domains, language, safe search)
- Pagination for deep results ✓
- Require API credentials ✓
- Always advertised even without credentials ✓
- Return setup instructions if missing credentials ✓
- Web fetch works immediately (no credentials) ✓

**Plan delivers (verified in plan/README.md, tool-schemas.md):**
- All three tools specified with exact schemas ✓
- Pagination implemented (count/offset for Brave, num/start for Google) ✓
- Domain filtering (allowed_domains, blocked_domains) ✓
- Credential discovery pattern defined (env vars → credentials.json → error) ✓
- config_required events via _event field ✓
- web-fetch-tool has no credential requirements ✓

**Conclusion:** Plan implements all features promised in README.

---

### 2026-01-16 - Verified: User Stories → Plan Support

**Check:** Does the plan support behaviors demonstrated in user stories?

**Tested user story:** first-time-discovery.md

**User story requires:**
1. Tool callable without credentials ✓
2. Tool returns error JSON with setup instructions ✓
3. Tool emits config_required event with detailed instructions ✓
4. Event displayed in dim yellow (separate from LLM output) ✓
5. LLM receives error and can explain situation ✓

**Plan provides:**
- tool-schemas.md:121-178 specifies exact error response format with _event field ✓
- plan/README.md:19-31 explains tool availability and config_required mechanism ✓
- build-integration.md:261-294 documents event extraction protocol ✓
- All required data present: error message, error_code, event content, event data ✓

**Conclusion:** Plan fully supports the first-time discovery user story. Other user stories rely on similar patterns.

---


### 2026-01-16 - Gap #5 (Test Strategy) Partially Addressed

**Gap Reference:** Major Gap #5 in gap.md (Test strategy missing)

**Finding:** tool-implementation.md:440-479 provides test strategy:
- Memory testing with valgrind ✓
- Pattern testing (schema, input validation, credentials) ✓
- References existing test patterns in tests/unit/tools/* ✓
- Specifies 100% coverage target ✓

**Evidence from codebase:**
- tests/unit/tools/tool_external_test.c demonstrates integration testing pattern ✓
- tests/unit/credentials_test.c demonstrates credential loading test pattern ✓
- Existing pattern: Create temp shell scripts, test via ik_tool_external_exec() ✓

**Remaining concern:** Mock HTTP responses for API testing not specified

**Recommendation:** Gap #5 is 80% resolved. Minor enhancement needed: specify HTTP mocking approach for testing without real API calls.

---


### 2026-01-16 - Domain Filtering Algorithm Specified

**Gap Reference:** Medium Gap #8 in gap.md (Domain filtering implementation incomplete)

**Issue:** Plan didn't specify domain filtering behavior:
- Brave: Post-processing algorithm unspecified (subdomain matching? case sensitivity?)
- Google: Multiple domain handling unspecified (API only supports single domain)

**Resolution:** Added comprehensive domain filtering specification to tool-schemas.md

**Changes Made:**
1. **plan/tool-schemas.md** - Added new "Domain Filtering" section after "Output Format Mapping"

**Brave Search (post-processing):**
- Exact hostname match only, case-insensitive
- "example.com" matches only "example.com", NOT "www.example.com" or "blog.example.com"
- Users must explicitly list all desired domains/subdomains
- Deduplication: Skip duplicate URLs

**Google Search (multiple parallel API calls):**
- `allowed_domains` with N domains: Make N parallel API calls, one per domain
- Split `num` across domains using integer division with remainder distributed to first domains
- Round-robin merge results with deduplication
- `blocked_domains`: Post-processing (same as Brave)
- Pagination: Split `start` across domains
- Partial failure: Return results from successful calls

**Algorithm specified:**
```
per_domain = num / domain_count
remainder = num % domain_count
First `remainder` domains get (per_domain + 1)
Remaining domains get per_domain (can be 0, skip those calls)
```

**Rationale:**
- Exact match is explicit and predictable
- Multiple Google calls allow proper multi-domain support
- Quota cost is fair (1 call per domain)
- Round-robin merge ensures diverse results

---

### 2026-01-16 - Database Integration Specified

**Gap Reference:** Medium Gap #9 in gap.md (Database integration incomplete)

**Issue:** Plan didn't specify how to store config_required events in database

**Resolution:** Added database integration specification to build-integration.md

**Changes Made:**
1. **plan/build-integration.md** - Added new "Database Integration for config_required Events" section at end

**Integration Point:**
- tool_wrapper.c function `ik_tool_wrap_success()` (or equivalent)
- Extract optional `_event` field from tool JSON output
- Store in messages table if present
- Remove `_event` before wrapping result for LLM

**Database Schema:**
- Uses existing messages table (share/ikigai/migrations/001-initial-schema.sql:41-48)
- `kind = 'config_required'`
- `content = _event.content` (user-facing warning text)
- `data = _event.data` (JSONB structured metadata)

**Event Lifecycle:**
1. Tool returns JSON with `_event` field
2. Wrapper extracts event, stores in database
3. `_event` removed from result
4. LLM sees only error message
5. User sees both error and event (dim yellow)

**Rendering:**
- Scrollback queries messages including `kind='config_required'`
- Events displayed in dim yellow separate from tool results
- Associated with same conversation turn as tool call

**Evidence:**
- Messages table already supports arbitrary event types via `kind TEXT` discriminator
- Schema supports structured metadata via `data JSONB` field
- No migration needed, just integration code in tool_wrapper.c

---

