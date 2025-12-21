---
name: task-authoring
description: Task Authoring skill for the ikigai project
---

# Task Authoring

Guidance for creating task files from requirements (user stories, bugs, gaps).

## Process

1. **Review source material** - Read and understand the requirements thoroughly
2. **Think deeply** - Consider edge cases, dependencies, and implementation approaches
3. **Break down** - Split into smallest achievable, testable units
4. **Create order.json** - Define execution order and metadata

## Abstraction Levels

### scratch/plan/ - Module Level

Plan documents describe **what modules exist and how they interact**:
- Module names and responsibilities
- Major structs (names and purpose, not full definitions)
- Key function signatures (declaration only)
- Data flow between modules

**DO NOT include:** Full struct definitions, implementation code, detailed algorithms.

### scratch/tasks/ - Interface Level

Task files specify **what to build**, not **how to build it**:
- Function signatures with purpose documentation
- Struct names with member descriptions (not C code)
- Expected behaviors and contracts
- Test scenarios (descriptions, not test code)

**DO NOT include:** Implementation code, complete struct definitions, working test code.

The sub-agent writes the actual code based on these specifications.

## File Naming

**DO NOT use numeric prefixes.** Order is defined in `order.json`, not filenames.

| Good | Bad |
|------|-----|
| `provider-types.md` | `01-provider-types.md` |
| `openai-adapter.md` | `02-openai-adapter.md` |
| `tests-integration.md` | `20-tests-integration.md` |

Use descriptive kebab-case names that reflect the task's purpose.

## order.json Structure

The `order.json` file is the **single source of truth** for task execution:

```json
{
  "todo": [
    {"task": "provider-types.md", "group": "Foundation", "model": "sonnet", "thinking": "none"},
    {"task": "openai-adapter.md", "group": "Migration", "model": "sonnet", "thinking": "thinking"},
    {"task": "tests-integration.md", "group": "Testing", "model": "sonnet", "thinking": "extended"}
  ]
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `task` | Yes | Filename (must exist in same directory) |
| `group` | Yes | Logical grouping for reporting |
| `model` | Yes | `sonnet` or `opus` |
| `thinking` | Yes | `none`, `thinking`, or `extended` |

### Serial Execution

**Tasks execute serially in array order.** Position 0 runs first, then 1, then 2, etc.

When adding a task, place it in the array such that all its dependencies come before it. The `Depends on:` field in task files is documentation only - the array position is what matters.

The database tracks completion state, not the JSON file.

## Task File Requirements

### Model/Thinking Selection

Choose minimum capability needed for each task:

| Complexity | Model | Thinking |
|------------|-------|----------|
| Straightforward | sonnet | none |
| Moderate | sonnet | thinking |
| Complex | sonnet | extended |
| Very complex | opus | extended |

Default to `sonnet/none` and escalate only when complexity demands it.

### Working Directory Context

**CRITICAL:** Every task file MUST include a Context section stating:
- Working directory is project root (where `Makefile` lives)
- All paths are relative to project root, not to the task file

This prevents sub-agents from misinterpreting paths like `scratch/plan/` as relative to `scratch/tasks/`.

### Skill Loading

- Load all skills the sub-agent will need (err on the side of more)
- Include relevant developer skills for implementation tasks
- **Never load `align`** - sub-agents execute, they don't negotiate

### Source Code Links

- Provide specific file paths the agent will need
- Include both implementation files and test patterns
- Link to related existing code as examples

### Pre/Post Conditions

- Pre-conditions must be verifiable before starting
- Post-conditions must be testable after completion
- Chain logically: post(N) = pre(N+1)

### Context (What/How/Why)

Every task must provide:

- **What** - The specific goal
- **How** - Approach and patterns to follow
- **Why** - Business/technical rationale

This context enables agents to handle unforeseen issues intelligently.

### Sub-agent Guidance

Include in task instructions:

- Suggest sub-agent use where appropriate (research, parallel work)
- Encourage persistence - overcome obstacles to complete the goal
- Task success is the measure, not partial progress

## Task Template

```markdown
# Task: [Descriptive Name]

**Model:** sonnet/thinking
**Depends on:** task-name.md (or "None")

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load skill-name` - Why needed

**Source:**
- `path/to/file.h` - What to learn from it

**Plan:**
- `scratch/plan/relevant.md` - Sections to reference

## Objective

One paragraph: what this task accomplishes and why.

## Interface

Functions to implement (signatures and contracts, not code):

| Function | Purpose |
|----------|---------|
| `res_t foo_create(...)` | Creates X, returns OK/ERR |
| `void foo_destroy(...)` | Cleanup, NULL-safe |

Structs to define (members and purpose, not C code):

| Struct | Members | Purpose |
|--------|---------|---------|
| `foo_t` | name, count, items | Represents X |

## Behaviors

- When X happens, do Y
- Error handling: return ERR with category Z
- Memory: talloc ownership rules

## Test Scenarios

- Create/destroy lifecycle
- Error case: invalid input returns ERR_INVALID_ARG
- Edge case: empty list handled

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
```
