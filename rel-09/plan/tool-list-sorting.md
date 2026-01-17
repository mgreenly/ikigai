# Tool List Sorting

Sort tool listing alphabetically after discovery.

## Problem

The `/tool` command displays tools in discovery order (filesystem iteration order), which is non-deterministic and depends on the underlying filesystem. This makes it difficult for users to locate specific tools in the list.

Example current output (unsorted):
```
Available tools:
  web_search_google (/path/to/web-search-google-tool)
  file_read (/path/to/file-read-tool)
  glob (/path/to/glob-tool)
  web_search_brave (/path/to/web-search-brave-tool)
  file_write (/path/to/file-write-tool)
  file_edit (/path/to/file-edit-tool)
  grep (/path/to/grep-tool)
  bash (/path/to/bash-tool)
```

## Solution

Sort the tool registry entries alphabetically by name once after discovery completes. This ensures consistent ordering across all tool registry operations without repeated sorting overhead.

**Where to sort:** After all three directories scanned in `ik_tool_discovery_run()`, before returning.

**Sort key:** Tool name (alphabetically, case-sensitive)

## Public Interface

### New Function

Add to `tool_registry.h`:

```c
// Sort registry entries alphabetically by name
void ik_tool_registry_sort(ik_tool_registry_t *registry);
```

**Behavior:**
- Sorts `registry->entries` array by `entry->name` (alphabetical, case-sensitive)
- In-place sort (no allocations)
- Safe to call on empty registry (no-op)
- Safe to call multiple times (idempotent)

### Modified Function

Update `tool_discovery.c` - `ik_tool_discovery_run()`:

After scanning all three directories (line 239), before `return OK(NULL)`:

```c
// Sort registry alphabetically by tool name
ik_tool_registry_sort(registry);

return OK(NULL);
```

## Data Flow

1. **Discovery phase:**
   - `ik_tool_discovery_run()` scans system/user/project directories
   - Tools added to registry in filesystem iteration order
   - **NEW:** After all scanning complete, call `ik_tool_registry_sort()`
   - Registry entries now sorted alphabetically by name

2. **Display phase:**
   - `/tool` command iterates `registry->entries` array
   - Tools displayed in sorted order (no additional sorting needed)

## Implementation Notes

**Sort algorithm:** Standard library `qsort()` with custom comparator.

**Comparator function:**
```c
static int tool_entry_compare(const void *a, const void *b)
{
    const ik_tool_registry_entry_t *entry_a = (const ik_tool_registry_entry_t *)a;
    const ik_tool_registry_entry_t *entry_b = (const ik_tool_registry_entry_t *)b;
    return strcmp(entry_a->name, entry_b->name);
}
```

**Sort call:**
```c
void ik_tool_registry_sort(ik_tool_registry_t *registry)
{
    if (registry->count > 1) {
        qsort(registry->entries, registry->count,
              sizeof(ik_tool_registry_entry_t), tool_entry_compare);
    }
}
```

**Why qsort:** No allocations, standard library, proven implementation.

## Affected Components

**Modified files:**
- `src/tool_registry.h` - Add `ik_tool_registry_sort()` declaration
- `src/tool_registry.c` - Add `ik_tool_registry_sort()` implementation
- `src/tool_discovery.c` - Call sort after scanning in `ik_tool_discovery_run()`

**No changes needed:**
- `src/commands_tool.c` - Already iterates registry in order
- Tool schemas - Unaffected
- Tool discovery logic - Unaffected (discovery order doesn't matter)

## Testing

**Unit tests:**
- `tests/unit/tools/tool_registry_test.c`:
  - Test sorting empty registry (no-op)
  - Test sorting single-entry registry (no-op)
  - Test sorting multi-entry registry (verify alphabetical order)
  - Test sorting already-sorted registry (idempotent)

**Integration tests:**
- Verify `/tool` displays sorted list after discovery
- Verify `/refresh` re-sorts after clearing registry

## Example Output

After implementation, `/tool` displays:

```
Available tools:
  bash (/path/to/bash-tool)
  file_edit (/path/to/file-edit-tool)
  file_read (/path/to/file-read-tool)
  file_write (/path/to/file-write-tool)
  glob (/path/to/glob-tool)
  grep (/path/to/grep-tool)
  web_search_brave (/path/to/web-search-brave-tool)
  web_search_google (/path/to/web-search-google-tool)
```

Tools alphabetically sorted by name, making specific tools easier to find.

## Rationale

**Why sort after discovery (not on display):**
- Discovery happens infrequently (startup, `/refresh`)
- Display happens frequently (`/tool` command, possibly other operations)
- Sorting once avoids repeated sorting overhead
- Consistent with user's explicit requirement: "sort after tool discovery"

**Why case-sensitive alphabetical:**
- All tool names currently lowercase (convention)
- Simple, predictable ordering
- Matches standard Unix tool listing conventions

**Why in-place sort:**
- No memory allocations needed
- Simpler error handling (void return, no OOM possible)
- Efficient for typical tool counts (< 50 tools)
