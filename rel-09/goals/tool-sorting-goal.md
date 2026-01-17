## Objective

Implement alphabetical sorting of tool registry entries after discovery completes, ensuring `/tool` command displays tools in consistent alphabetical order.

## Reference

**Plan Documents:**
- $CDD_DIR/plan/tool-list-sorting.md - Complete specification for sorting implementation
- $CDD_DIR/plan/README.md - Tool architecture overview

**Existing Implementation:**
- src/tool_registry.h - Tool registry data structures (ik_tool_registry_t, ik_tool_registry_entry_t)
- src/tool_registry.c - Registry management functions (create, add, clear, lookup, build_all)
- src/tool_discovery.c - Tool discovery implementation (ik_tool_discovery_run scans directories)
- src/commands_tool.c - /tool command implementation (ik_cmd_tool iterates registry)

**Testing Examples:**
- tests/unit/tools/tool_registry_test.c - Existing registry unit tests
- tests/unit/commands/commands_tool_test.c - Existing /tool command tests

**User Story:**
- project/archive/rel-08/user-stories/list-tools.md - Expected /tool command behavior

## Outcomes

**New Function:**
- `ik_tool_registry_sort()` added to src/tool_registry.h (declaration)
- `ik_tool_registry_sort()` implemented in src/tool_registry.c
- Function sorts registry->entries array alphabetically by entry->name
- Uses qsort() with custom comparator (case-sensitive strcmp)
- Safe on empty registry (no-op when count == 0 or count == 1)
- No memory allocations (in-place sort)
- Void return (no error possible)

**Integration:**
- src/tool_discovery.c modified in `ik_tool_discovery_run()`
- Sort called after all three directory scans complete (after line 239)
- Sort called before `return OK(NULL)`
- Order: scan system → scan user → scan project → **sort** → return

**Testing:**
- tests/unit/tools/tool_registry_test.c extended with sort tests:
  - TEST(tool_registry, sort_empty) - Verify no-op on empty registry
  - TEST(tool_registry, sort_single) - Verify no-op on single entry
  - TEST(tool_registry, sort_multiple) - Verify alphabetical ordering
  - TEST(tool_registry, sort_idempotent) - Verify repeated sort is safe
- All existing tests still pass (sorting doesn't break functionality)

**Behavior:**
- `/tool` command displays tools in alphabetical order by name
- Order consistent across invocations (deterministic)
- Order consistent after `/refresh` (re-sorts after re-discovery)

## Acceptance

**Build Success:**
- `make` completes without errors
- `make check` completes without errors
- No compiler warnings

**Function Signature:**
- src/tool_registry.h declares: `void ik_tool_registry_sort(ik_tool_registry_t *registry);`
- Function implemented in src/tool_registry.c
- Uses qsort with comparator function

**Integration:**
- src/tool_discovery.c calls `ik_tool_registry_sort(registry)` after scanning
- Call happens after all three directories scanned (line ~239-240)
- Call happens before return statement

**Sorted Output:**
- After running ikigai, type `/tool`
- Tools displayed alphabetically by name
- Example order: bash, file_edit, file_read, file_write, glob, grep, web_search_brave, web_search_google
- Verify tools appear in strcmp-sorted order (case-sensitive alphabetical)

**Tests Pass:**
- `make check` passes with 100% coverage maintained
- New tests in tests/unit/tools/tool_registry_test.c:
  - sort_empty test passes
  - sort_single test passes
  - sort_multiple test passes (verifies alphabetical order)
  - sort_idempotent test passes
- All existing tool_registry tests still pass
- All existing commands_tool tests still pass

**No Regressions:**
- Tool discovery still works (tools load correctly)
- Tool invocation still works (LLM can call tools)
- `/tool <name>` still works (show specific tool schema)
- `/refresh` still works (re-discovers and re-sorts)
