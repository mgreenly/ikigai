# Task: Define Tool Call Data Structure

## Target
User story: 02-single-glob-call

## Agent
model: haiku

## Pre-conditions
- `make check` passes
- Task `request-with-tools.md` completed (Story 01 complete)
- `src/tool.h` and `src/tool.c` exist with schema functions

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/naming.md (ik_module_function conventions)
- src/tool.h (existing tool module)
- rel-04/user-stories/02-single-glob-call.md (see Response A for tool_calls format)

## Task
Define `ik_tool_call_t` struct to hold a parsed tool call. This struct will store the id, function name, and arguments extracted from the API response.

Expected structure based on Response A:
```c
typedef struct {
    char *id;         // "call_abc123"
    char *name;       // "glob"
    char *arguments;  // "{\"pattern\": \"*.c\", \"path\": \"src/\"}"
} ik_tool_call_t;
```

## TDD Cycle

### Red
1. Add test in `tests/unit/tool/test_tool.c`:
   - Test `ik_tool_call_create()` returns non-NULL
   - Test fields are set correctly (id, name, arguments)
   - Test NULL parent works (talloc root context)
2. Run `make check` - expect compile failure (function doesn't exist)

### Green
1. Add `ik_tool_call_t` typedef to `src/tool.h`
2. Add `ik_tool_call_create(void *parent, const char *id, const char *name, const char *arguments)` declaration
3. Implement in `src/tool.c`:
   - Allocate struct with talloc
   - Copy strings with talloc_strdup (owned by struct)
   - Return pointer
4. Run `make check` - expect pass

### Refactor
1. Verify naming matches docs/naming.md
2. Ensure memory ownership is clear (struct owns its strings)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_call_t` struct and `ik_tool_call_create()` exist
- 100% test coverage for new code
