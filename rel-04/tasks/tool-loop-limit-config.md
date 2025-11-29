# Task: Tool Loop Limit Configuration

## Target
User story: 11-tool-loop-limit-reached

## Agent
model: haiku

## Pre-conditions
- `make check` passes
- Multi-tool conversation loop works (Story 04: multi-tool-loop.md completed)
- Conversation loop auto-continues until finish_reason is "stop"

## Context
Read before starting:
- src/config.h or src/main.c (application configuration)
- docs/opus/architecture.md (system design)
- rel-04/user-stories/11-tool-loop-limit-reached.md (full user story)

## Task
Add a configurable limit for maximum tool call iterations in a single user request. This prevents infinite loops when the model makes repeated tool calls.

Add a constant or configuration value `MAX_TOOL_ITERATIONS` with default value of 3. This will be used to track how many times the loop has executed tool calls before requiring the model to provide a final response.

## TDD Cycle

### Red
1. Add test to existing configuration test suite (or create new test if none exists):
   - Test that MAX_TOOL_ITERATIONS constant/config value exists
   - Test that default value is 3
2. Run `make check` - expect compile failure if constant doesn't exist

### Green
1. Add `MAX_TOOL_ITERATIONS` constant to appropriate header file (e.g., src/config.h, src/repl.h, or src/tools/tool.h)
2. Set default value to 3
3. Run `make check` - expect pass

### Refactor
1. Consider if this should be:
   - A compile-time constant (#define)
   - A runtime configuration option (struct field)
   - Consider future extensibility (command-line override, per-session config)
2. Ensure naming follows docs/naming.md conventions
3. Add brief comment explaining purpose
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- MAX_TOOL_ITERATIONS constant exists with value 3
- 100% test coverage for new code
