# Agent Scripts Architecture

## Problem Statement

Model Context Protocol (MCP) requires significant context overhead:
- Full protocol specifications
- Schema definitions for each tool
- Tool interface descriptions
- Request/response formats

This consumes valuable context tokens that could be used for actual problem-solving.

## Solution: Script-Based Tools

Provide agent functionality through executable scripts with standardized interfaces:
- Scripts live in `.agents/scripts/`
- Each script has structured markdown documentation
- All scripts return JSON with consistent format
- Models use Bash to execute scripts
- Scripts can wrap MCP servers, APIs, or local processing (transparent to model)

## Design Decisions

### TypeScript/Deno over Bash

**Rationale**: While bash is simple, TypeScript/Deno provides:
- Native JSON handling (vs string manipulation)
- Built-in HTTP/fetch (vs curl parsing)
- Proper error handling (try/catch vs exit codes)
- Type safety catches bugs early
- Rich standard library (file ops, crypto, etc.)
- Single-binary execution (`deno run script.ts`)
- Better maintainability for complex logic

**Trade-off**: Requires Deno runtime, but complexity benefits outweigh installation cost.

### Bash Execution (Not Custom Tool)

**Decision**: Models execute scripts via Bash, not through a dedicated `deno` tool.

**Rationale**:
- Simpler architecture - no new tool to maintain
- Full transparency - user sees exact command being run
- Natural flexibility - models can chain, pipe, redirect
- Consistency - same as manual execution
- Deno's permission model is explicit in commands
- Models already excel at bash string escaping

**Example**:
```bash
deno run --allow-read .agents/scripts/check-coverage/run.ts src/config.c
```

Permissions (`--allow-read`) are visible in README documentation and commands, not hidden behind abstraction.

### Structured Documentation Pattern

Each script directory contains `README.md` with standardized sections:

```markdown
# Tool Name

## Summary
One-line description for quick scanning

## Usage
```bash
cd /path/to/ikigai
deno run --allow-net .agents/scripts/tool-name/run.ts <args>
```

## Arguments
| Name | Required | Description |
|------|----------|-------------|
| arg1 | yes | Description |
| --flag | no | Optional flag (default: value) |

## Output
Returns JSON to stdout:
```json
{
  "success": true,
  "data": { "key": "value" }
}
```

## Error Codes
| Code | Description |
|------|-------------|
| ERROR_CODE | What it means |

## Examples
[1-2 concrete examples with common use cases]
```

**Why This Works**:
- Models can `ls .agents/scripts/` to discover tools
- Models read README.md to understand usage
- Structured sections (tables, code blocks) are easy to parse
- Human-readable documentation too
- Shows exact command to copy, including permissions

### JSON Output Contract

**Standard format for all scripts**:

```typescript
// Success
{
  "success": true,
  "data": { /* script-specific data */ }
}

// Error
{
  "success": false,
  "error": "Human-readable error description",
  "code": "MACHINE_READABLE_ERROR_CODE"
}
```

**Benefits**:
- Predictable error handling across all scripts
- Models can check `success` field consistently
- Error codes enable programmatic retry logic
- Exit codes: 0 for success, 1 for errors

## Agent Discovery Workflow

1. **Discovery**: `ls .agents/scripts/` → see available tools
2. **Understanding**: `cat .agents/scripts/<tool>/README.md` → learn usage
3. **Execution**: Run exact command from README documentation
4. **Parsing**: Parse JSON from stdout
5. **Error Handling**: Check `success`; if false, handle `error`/`code`

This is far more context-efficient than loading full MCP schemas.

## Slash Command Integration

**`/execute-script` command**: User-initiated script execution

**Use cases**:
- When user knows what data is needed (no LLM round trip)
- Context priming: run script first, then ask model to interpret results
- Verification: user sees script output before model interprets
- Cost savings: no tokens wasted on model deciding to run script

**Example**:
```bash
/execute-script .agents/scripts/check-coverage/run.ts src/config.c
# Results injected as system message
# Then user asks: "What's missing coverage and why?"
```

## Available Scripts

### check-coverage

**Location**: `.agents/scripts/check-coverage/`

**Purpose**: Check code coverage for a specific source file, identify uncovered lines/branches

**Usage**:
```bash
deno run --allow-read .agents/scripts/check-coverage/run.ts <source-file>
```

**Output** (abbreviated):
```json
{
  "success": true,
  "data": {
    "file": "src/config.c",
    "summary": {
      "line_coverage_percent": 100.0,
      "branch_coverage_percent": 98.7,
      "uncovered_lines": 0,
      "uncovered_branches": 1
    },
    "uncovered": {
      "lines": [],
      "branches": [{"line": 56, "block": 0, "branch": 1}]
    }
  }
}
```

**Implementation highlights**:
- Parses LCOV format (`coverage/coverage.info`)
- TypeScript interfaces for type safety
- Comprehensive error codes
- Handles file matching by suffix (`config.c` finds `src/config.c`)
- Can pipe to `jq` for specific data extraction

### Task System Scripts

**Location**: `.agents/scripts/`

**Purpose**: Manage hierarchical task execution with state tracking

**Scripts**:
- `task-list/run.ts` - List and sort tasks from a directory
- `task-next/run.ts` - Get next task based on current state
- `task-start/run.ts` - Start a task (mark as in_progress)
- `task-done/run.ts` - Mark task complete (awaiting verification)
- `task-verify/run.ts` - Verify and advance to next task

**See**: [task-system.md](task-system.md) for comprehensive documentation on the task system architecture, workflow, and usage.

## Benefits

1. **Context efficiency** - Minimal upfront token cost, discovery on-demand
2. **Transparency** - Full command visibility for security/debugging
3. **Flexibility** - Scripts can wrap any backend (MCP, API, local)
4. **Maintainability** - Standard TypeScript, easy to test/version
5. **Parallel execution** - Bash handles concurrent script execution naturally
6. **Caching** - Scripts can cache results locally as needed
7. **Developer experience** - Easy to run scripts manually for testing

## Future Considerations

- **Priming strategy**: List all tools in `default.md` vs runtime discovery?
  - Hybrid approach: mention `.agents/scripts/` exists, discover as needed
  - Balances context efficiency with discoverability

- **Script organization**: As scripts grow, consider subdirectories
  - `.agents/scripts/coverage/check.ts`
  - `.agents/scripts/database/query.ts`

- **Standard library**: Common TypeScript utilities shared across scripts
  - Error formatting helpers
  - JSON output builders
  - Common validation patterns

## Integration Points

- **REPL slash commands**: `/execute-script <path> [args]`
- **Model tool use**: Bash tool executes scripts based on README docs
- **Configuration**: May need `.agents/config.json` for script settings
- **Permissions**: Deno permission flags documented per-script in README

---

**Philosophy**: Play to the model's strengths (bash, directory navigation, markdown parsing) while minimizing context overhead through conventions over protocols.
