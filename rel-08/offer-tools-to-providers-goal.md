## Objective

Wire the tool registry to LLM provider requests. After completion, tools discovered at startup appear in LLM API calls, and tool execution uses the external tool system.

Currently, `ik_request_build_from_conversation()` has a TODO placeholder (line 145-148) where tools should be populated from `shared->tool_registry`. The registry is already populated by discovery at startup, but tools are not being offered to providers.

## Reference

- `rel-08/plan/integration-specification.md` - Exact struct changes, function signature changes, call site locations
- `rel-08/plan/architecture.md` - Schema transformation rules per provider (OpenAI strict mode, Anthropic input_schema, Google functionDeclarations)
- `rel-08/plan/test-specification.md` - Test file locations and scenarios (Phase 4 section)
- `src/tool_registry.h` - Registry types and `ik_tool_registry_build_all()`
- `src/providers/request_tools.c` - Current TODO placeholder at line 145-148

## Outcomes

**Request building:**
- `ik_request_build_from_conversation()` signature updated to accept `ik_tool_registry_t *registry` parameter
- Iterates registry entries, extracts name/description/parameters from schema JSON
- Calls `ik_request_add_tool()` for each registry entry
- All call sites updated to pass `agent->shared->tool_registry`

**Tool execution:**
- `repl_tool.c` uses `ik_tool_registry_lookup()` + `ik_tool_external_exec()` instead of removed `ik_tool_dispatch()`
- Results wrapped via `ik_tool_wrap_success()` / `ik_tool_wrap_failure()`

**Provider serialization:**
- No changes needed - serializers already read from `req->tools[]` and transform correctly
- OpenAI strict mode transformation already implemented and tested

**Test updates:**
- `tests/unit/providers/request_tools_schema_test.c` updated: expects tools from registry instead of `tool_count == 0`
- New test(s): create mock registry with known entries, verify `req->tools[]` populated correctly after `ik_request_build_from_conversation()`
- Mock pattern: use `ik_tool_registry_create()` + `ik_tool_registry_add()` with test schema JSON docs (no discovery mocking needed - registry is a plain data structure)

## Acceptance

Run checks in order. All must return `{"ok": true}`:

1. `.claude/harness/compile/run`
2. `.claude/harness/filesize/run`
3. `.claude/harness/check/run`
4. `.claude/harness/complexity/run`
5. `.claude/harness/sanitize/run`
6. `.claude/harness/tsan/run`
7. `.claude/harness/valgrind/run`
8. `.claude/harness/helgrind/run`
9. `.claude/harness/coverage/run`

## Skills

Load `memory` skill if talloc ownership between registry and request becomes unclear.
