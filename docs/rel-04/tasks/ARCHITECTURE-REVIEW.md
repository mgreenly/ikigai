# Architecture Review - rel-04 Task Plan

Six-perspective review of task plan for architectural and design issues.

---

## Summary

| Perspective | Status | Actionable Issues |
|-------------|--------|-------------------|
| Dependency Graph | ✅ CLEAN | 0 |
| API Surface | ✅ RESOLVED | 0 |
| Data Flow | ✅ RESOLVED | 0 |
| State & Ownership | ✅ RESOLVED | 0 |
| Error Strategy | ✅ RESOLVED | 0 |
| Conceptual Coherence | ✅ CLEAN | 0 |

**Total actionable issues: 0** (was 8; all resolved or not issues)

---

## Critical Issues

### 1. Glob Execution Signature Mismatch

**Perspective**: API Surface
**Severity**: CRITICAL
**File**: `glob-execute.md` (line 58)

**Problem**: Task specifies signature with extracted parameters:
```c
ik_tool_exec_glob(void *parent, const char *pattern, const char *path)
```

But all other tools and the dispatcher expect:
```c
ik_tool_exec_glob(void *parent, const char *arguments)  // JSON string
```

**Impact**: Glob won't work with `ik_tool_dispatch()` which passes JSON arguments.

**Fix**: Update `glob-execute.md` Green phase to use `(void *parent, const char *arguments)` signature, parsing arguments inside the function using `ik_tool_arg_get_string()`.

**Status: RESOLVED** - Chose alternative design: dispatcher parses JSON and extracts typed parameters. Updated all tool executor tasks (`file-read-execute.md`, `bash-execute.md`, `file-write-execute.md`, `grep-execute.md`) to receive typed parameters. Updated `tool-dispatcher.md` to handle JSON parsing centrally.

---

### 2. Missing Rollback on Partial Tool Execution Failure

**Perspective**: Error Strategy
**Severity**: CRITICAL
**File**: `tool-loop-state-mutation.md`

**Problem**: When the tool loop executes:
1. Add assistant message with tool_calls → SUCCESS
2. Execute tool → SUCCESS
3. Add tool result message → FAILS (DB error)

Conversation now has orphaned tool_calls with no result. Follow-up request sends incomplete context.

**Fix**: Add to `tool-loop-state-mutation.md` Red phase:
- Test case: "If tool result persist fails, assistant message is removed from conversation"
- Implement rollback or wrap in transaction

**Status: NOT AN ISSUE** - Tool request and tool result are two independent events by design. Memory is authoritative during a session; the database is an event log. If one persist fails, the session continues (memory has both). Orphaned events on session restore are rare (crash + partial persist) and the model recovers gracefully. No transactional coupling needed. See rel-04/README.md "Database Persistence" section.

---

### 3. Tool Error Result Format Inconsistent

**Perspective**: Error Strategy
**Severity**: HIGH
**Files**: `bash-execute.md`, `file-read-execute.md`, `grep-execute.md`

**Problem**: Tools return different error structures:
```json
// bash: success format with failed code
{"output": "...", "exit_code": 1}

// file_read: explicit error
{"error": "File not found"}
```

**Impact**: `ik_openai_msg_from_db()` must handle variable formats; no unified error detection.

**Fix**: Define unified error format in a new section of `tool-output-limit.md` or create `docs/tool-result-format.md`:
```json
{"success": false, "error": "message", "error_code": "IO"}
```

**Status: RESOLVED** - Created unified tool result envelope format. All tools now return:
- Success: `{"success": true, "data": {...}}`
- Error: `{"success": false, "error": "message"}`

See `rel-04/docs/tool-result-format.md` for full specification and `rel-04/docs/tool_use.md` for implementation guide. Updated all user stories and execute task files to use this format.

---

## High Priority Issues

### 4. Message Type Polymorphism Unclear

**Perspective**: Data Flow
**Severity**: HIGH
**Files**: `assistant-tool-calls-msg.md`, `tool-result-msg.md`

**Problem**: `ik_openai_msg_t` must represent 4 incompatible shapes:
- User/System/Assistant with content
- Assistant with tool_calls (no content)
- Tool with tool_call_id

Tasks don't specify whether to:
- Extend single struct with conditional fields
- Use union type
- Use separate struct types

**Fix**: Add to `assistant-tool-calls-msg.md` Pre-read section:
- Specify struct design choice (recommend union or separate types)
- Document which fields are active for each role
- Add serialization rules

**Status: RESOLVED** - Adopted canonical message format instead of extending OpenAI-specific structs. The in-memory conversation uses provider-agnostic `ik_msg_t` with `kind` discriminator (system, user, assistant, tool_call, tool_result). Each provider module transforms canonical → wire format during serialization.

Updated files:
- `rel-04/README.md` - Added "Canonical Message Format" section
- `tool-result-msg.md` - Uses canonical format with kind="tool_result"
- `assistant-tool-calls-msg.md` - Uses canonical format with kind="tool_call"
- `conversation-rebuild.md` - Loads canonical messages (no transformation needed)
- `tool-loop-state-mutation.md` - Conversation uses canonical format

---

### 5. Tool Loop Error Continuation Ambiguous

**Perspective**: Error Strategy
**Severity**: HIGH
**Files**: `tool-loop-state-mutation.md`, `multi-tool-loop.md`

**Problem**: No specification for:
- What if tool execution returns error result?
- Should loop continue or stop?
- What if first tool succeeds but second fails?

**Fix**: Add explicit step to `tool-loop-state-mutation.md`:
- "Tool execution error → create error tool_result message → add to conversation → loop continues"
- Add test case to `multi-tool-loop.md` for mixed success/failure sequence

**Status: RESOLVED** - No special handling needed. There is no "loop control" decision - tool errors are just data returned to the LLM. The LLM receives the error result and decides how to respond (retry, explain to user, etc.). User stories 08 and 09 already cover tool error scenarios:
- `08-file-not-found-error.md`: Tool returns `{"success": false, "error": "..."}` → LLM explains to user
- `09-bash-command-fails.md`: Command returns non-zero exit code → LLM explains to user

Corresponding tasks (`file-read-error-e2e.md`, `bash-command-error-e2e.md`) verify this behavior end-to-end.

---

### 6. Conversation Rebuild Missing JSON Validation

**Perspective**: Error Strategy
**Severity**: HIGH
**File**: `conversation-rebuild.md` (lines 81-98)

**Problem**: `ik_openai_msg_from_db()` parses `data_json` but no error handling for:
- Malformed JSON
- Missing required fields
- Unknown message kinds

**Fix**: Add to `conversation-rebuild.md` Red phase:
- Test: "`data_json` with invalid JSON → returns ERR_PARSE"
- Specify behavior: reject message? log warning? skip?

**Status: NOT AN ISSUE** - Schema version checking makes this redundant:
1. App checks schema version on startup, refuses to start if schema > supported
2. Unknown message kinds from future versions → impossible (app wouldn't start)
3. Malformed JSON → impossible (JSONB validates on insert)
4. Missing required fields → bug in our own code (caught in testing)
5. Data corruption → bigger problems; validation won't save you

The schema version check is the gatekeeper. Adding defensive validation would be over-engineering.

---

## Medium Priority Issues

### 7. Kind→Role Mapping Not Centralized

**Perspective**: Data Flow
**Severity**: MEDIUM
**File**: `conversation-rebuild.md`

**Problem**: Database `kind` to OpenAI `role` mapping is implicit:
```
"system"     → "system"
"user"       → "user"
"assistant"  → "assistant"
"tool_call"  → "assistant" (with tool_calls)
"tool_result"→ "tool"
```

Scattered across tasks; changing mapping requires updates in multiple places.

**Fix**: Add to `conversation-rebuild.md` Green phase:
- Create explicit mapping table or enum
- Single source of truth for kind/role transformation

**Status: RESOLVED** - With the canonical message format, kind→role mapping is centralized in the OpenAI serializer (`ik_openai_serialize_request()`). The conversation uses canonical format with `kind`; transformation to provider-specific wire format happens in one place per provider. See `rel-04/README.md` "Message Transformation (Canonical to Wire Format)" section.

---

### 8. Tool Argument Parsing Responsibility Unclear

**Perspective**: Data Flow
**Severity**: MEDIUM
**File**: `tool-dispatcher.md`

**Problem**: Tasks don't specify who parses the JSON arguments string:
- Dispatcher receives `const char *arguments` (JSON string)
- Is parsing in dispatcher or in each executor?
- What's the error response for invalid JSON?

**Fix**: Add clarification to `tool-dispatcher.md`:
- "Dispatcher does NOT parse arguments"
- "Each executor parses its own arguments using `ik_tool_arg_get_*()`"
- "Invalid JSON → return error result JSON"

**Status: RESOLVED** - Documented in `rel-04/docs/tool_use.md` under "Key Decisions → Dispatcher Parses JSON Arguments": The dispatcher extracts typed parameters from the JSON arguments string and passes them to executors. Executors receive typed C values, not raw JSON.

---

## Informational (No Action Required)

### Dependency Graph
- No circular dependencies
- Clear module boundaries (tool → openai/db, repl orchestrates)
- Task sequence respects dependencies

### Conceptual Coherence
- Terminology consistent across all 39 tasks
- Mental models are coherent and well-layered
- Naming follows docs/naming.md conventions

### State & Ownership
- Talloc integration is excellent
- Error handling uses res_t consistently
- Session tracking is clear

---

## Next Steps

1. ~~Fix Critical #1 (glob signature)~~ - RESOLVED
2. ~~Fix Critical #2 (rollback)~~ - NOT AN ISSUE (independent events by design)
3. ~~Fix Critical #3 (error format)~~ - RESOLVED
4. ~~Address High #4 (message polymorphism)~~ - RESOLVED (canonical message format)
5. ~~Address High #5 (tool loop error continuation)~~ - RESOLVED (covered by stories 08-09)
6. ~~Address High #6 (conversation rebuild JSON validation)~~ - NOT AN ISSUE (schema version check is gatekeeper)
7. ~~Address Medium #7 (kind→role mapping)~~ - RESOLVED (by #4)
8. ~~Address Medium #8 (argument parsing)~~ - RESOLVED (documented in tool_use.md)

**All issues resolved.** Architecture review complete.
