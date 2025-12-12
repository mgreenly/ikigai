# Task: Define spawn_sub_agent Tool Schema

**Target**: Agent-Spawned Sub-Agents

**Agent model**: haiku

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Source patterns
- `src/tools/tool.h` - Tool definition patterns
- `src/tools/tool_*.c` - Existing tool implementations

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- Tool infrastructure exists
- Agent hierarchy fields exist

## Task

Define the `spawn_sub_agent` tool schema:

```json
{
  "type": "function",
  "function": {
    "name": "spawn_sub_agent",
    "description": "Create a sub-agent to handle a delegated task. Blocks until sub-agent completes and returns its result.",
    "parameters": {
      "type": "object",
      "properties": {
        "system_prompt": {
          "type": "string",
          "description": "System instructions defining the sub-agent's role and constraints"
        },
        "prompt": {
          "type": "string",
          "description": "The task or question for the sub-agent to address"
        }
      },
      "required": ["prompt"]
    }
  }
}
```

Add to tool registry so LLM can invoke it.

## TDD Cycle

### Red
Write test verifying tool schema is registered and JSON is valid.

### Green
Add schema definition to tool registry.

### Verify
`make check` passes.

## Post-conditions

- `spawn_sub_agent` tool registered
- Schema matches specification
- All tests pass
- Working tree is clean (all changes committed)
