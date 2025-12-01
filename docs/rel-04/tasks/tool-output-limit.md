# Task: Define Tool Output Truncation Utility

## Target
User story: 02-single-glob-call

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/naming.md
- docs/memory.md
- rel-04/README.md (see Configuration section for max_output_size)

### Pre-read Source (patterns)
- src/config.h (config field patterns)
- src/tool.h (existing tool module)

### Pre-read Tests (patterns)
- tests/unit/tool/test_tool.c (test structure pattern)

## Pre-conditions
- `make check` passes
- Task `tool-call-struct.md` completed
- Task `tool-config-fields.md` completed
- `ik_tool_call_t` struct exists
- `ik_cfg_t` has `max_output_size` field

## Task
Implement a shared output truncation utility that enforces `max_output_size` uniformly across all tool results. This utility will be used by glob, file_read, grep, and bash execute functions.

The truncation utility should:
1. Read `max_output_size` from config (default: 1048576 bytes / 1MB)
2. Truncate output if it exceeds the limit
3. Append a truncation indicator when output is truncated

Truncation indicator format:
```
[Output truncated: showing first 1048576 of 2500000 bytes]
```

## TDD Cycle

### Red
1. Add tests in `tests/unit/tool/test_tool.c`:
   - Test `ik_tool_truncate_output()` with output under limit (returns unchanged)
   - Test with output exactly at limit (returns unchanged)
   - Test with output over limit (truncates and adds indicator)
   - Test NULL output returns NULL
   - Test empty string returns empty string
2. Add declaration to `src/tool.h`: `ik_tool_truncate_output(void *parent, const char *output, size_t max_size)` returning `char *`
3. Add stub in `src/tool.c`: `return NULL;`
4. Run `make check` - expect assertion failure (returns NULL, test expects valid string)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - If output is NULL, return NULL
   - If output length <= max_size, return talloc_strdup of output
   - If output length > max_size, truncate and append indicator
   - Allocate result with talloc under parent context
3. Run `make check` - expect pass

### Refactor
1. Verify naming matches docs/naming.md
2. Ensure memory ownership is clear (caller owns returned string)
3. Consider edge cases (max_size = 0)
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_truncate_output()` function exists and is tested
- 100% test coverage for new code

## Usage Notes
All tool execute functions (glob, file_read, grep, bash) should call this utility before returning their output. This ensures consistent truncation behavior across all tools.
