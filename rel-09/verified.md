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
**Resolved:** 5 gaps
**Remaining:** 4 gaps
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
