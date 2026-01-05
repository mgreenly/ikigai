# Task: Tool Commands (/tool, /refresh)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** provider-openai.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task implements user-facing commands for tool inspection and management.

## Pre-Read

**Skills:**
- `/load errors` - res_t patterns
- `/load style` - Code style
- `/load naming` - ik_cmd_* naming

**Plan:**
- `cdd/plan/architecture.md` - Section "Commands" for signatures and behavior

**Source:**
- `src/commands.c` - Command registry pattern
- `src/commands_basic.c` - Example command implementations
- `src/commands_model.c` - Command with arguments example

## Libraries

Use only existing libraries. No new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] All provider integrations complete
- [ ] Registry populated with tools

## Objective

Implement two new commands:
1. `/tool` - List or inspect tools
2. `/refresh` - Reload tool registry

## New Files

### src/commands_tool.h

```c
#ifndef IK_COMMANDS_TOOL_H
#define IK_COMMANDS_TOOL_H

#include "error.h"

struct ik_repl_ctx_t;

res_t ik_cmd_tool(void *ctx, struct ik_repl_ctx_t *repl, const char *args);
res_t ik_cmd_refresh(void *ctx, struct ik_repl_ctx_t *repl, const char *args);

#endif
```

### src/commands_tool.c

Implement both commands.

## Command Behavior

### /tool (no args)

List all tools with names and descriptions:

```
Available tools:
  bash - Execute a shell command and return output
  file_read - Read contents of a file
  file_write - Write content to a file
  file_edit - Edit a file by replacing text
  glob - Find files matching a pattern
  grep - Search for pattern in files
```

### /tool NAME

Show full schema for named tool:

```
Tool: bash
Description: Execute a shell command and return output

Parameters:
  command (string, required): Shell command to execute
```

### /tool UNKNOWN

Error message:

```
Unknown tool: 'foo'
Use /tool to see available tools.
```

Return `ERR(ctx, ERR_INVALID_ARG, ...)` after displaying error.

### /refresh

1. Display "Refreshing tools..."
2. Clear existing registry (or create new one)
3. Call `ik_tool_discovery_run()` (blocking)
4. Display "Tools refreshed. N tools available."

## Command Registration

In `src/commands.c`, add to command table:

```c
{"tool", "List or inspect tools (usage: /tool [NAME])", ik_cmd_tool},
{"refresh", "Reload tool registry from disk", ik_cmd_refresh},
```

## Implementation Notes

1. Access registry via `repl->shared->tool_registry`
2. Output to scrollback via existing command patterns
3. Commands are local (no LLM call)
4. `/refresh` is blocking - user expects immediate feedback

## Files to Modify

- `src/commands.c` - Add command registration
- `Makefile` - Add `src/commands_tool.c` to sources

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(commands.md): [success|partial|failed] - tool commands

Implemented /tool and /refresh commands.
EOF
)"
```

Report status:
- Success: `/task-done commands.md`
- Partial/Failed: `/task-fail commands.md`

## Postconditions

- [ ] `/tool` lists all 6 tools
- [ ] `/tool bash` shows bash schema
- [ ] `/tool unknown` shows error message
- [ ] `/refresh` reloads registry
- [ ] `make check` passes
- [ ] All changes committed
- [ ] Working copy is clean
