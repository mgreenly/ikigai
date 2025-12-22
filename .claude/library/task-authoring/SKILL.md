---
name: task-authoring
description: Task Authoring skill for the ikigai project
---

# Task Authoring

Guidance for creating task files from requirements (user stories, bugs, gaps).

## Critical Context

**Task execution is UNATTENDED. No human in the loop.**

When `/orchestrate` runs, it executes all tasks automatically. If a sub-agent needs context you didn't provide, it will:
- Fail and escalate (wasting attempts)
- Succeed partially (creating technical debt)
- Spawn research agents (ballooning token usage)

**There is no one to ask for help. The task file IS the help.**

## Efficiency Principle

**Spend generously during task authoring to save massively during execution.**

Task authoring happens ONCE. Task execution happens N times (once per task in the orchestration loop). Therefore:

- **During authoring:** Use research agents, explore codebase, read extensively, think deeply
- **During execution:** Sub-agent has everything, executes immediately, no exploration needed

**Token math:**
- Poor task authoring: 10K tokens → Each of 20 tasks researches → 200K+ execution tokens
- Good task authoring: 50K tokens → 20 tasks execute directly → 40K execution tokens

**Completeness requirement:**
- Poor task: "Implement feature X" → Sub-agent fails or researches (unattended failure)
- Good task: Lists skills, files, interface specs, patterns, edge cases → Sub-agent executes to completion

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

**CRITICAL: Provide EXACTLY what the sub-agent needs - no more, no less.**

- List the specific skills needed for THIS task only
- Don't load large reference skills (database, source-code) unless task directly requires them
- Don't assume sub-agent will research or explore - give them what they need upfront
- **Never load `align`** - sub-agents execute, they don't negotiate

**Goal:** Sub-agent executes immediately with provided context. No exploration, no research sub-agents.

### Source Code Links

**Provide ALL file paths the sub-agent needs - be complete.**

- List specific implementation files to read/modify
- List specific test files to follow as patterns
- List related existing code as examples
- Be exhaustive - sub-agent should NOT need to search or explore

### Pre/Post Conditions

- Pre-conditions must be verifiable before starting
- Post-conditions must be testable after completion
- Chain logically: post(N) = pre(N+1)

### Context (What/How/Why)

**Every task must provide complete context so sub-agent can execute immediately.**

- **What** - The specific goal with full details
- **How** - Exact approach, patterns, and implementation guidance
- **Why** - Business/technical rationale for decision-making

**Goal:** Sub-agent has everything needed to execute. No need to research background, explore alternatives, or understand broader context.

### Execution Expectations

**UNATTENDED EXECUTION: Provide everything needed for autonomous completion.**

Tasks execute without human oversight. The sub-agent cannot ask questions or request clarification. Everything it needs must be in the task file.

Checklist for complete task authoring:
- [ ] All required skills listed explicitly
- [ ] All file paths to read/modify listed
- [ ] All test patterns and examples referenced
- [ ] Edge cases and error conditions documented
- [ ] Success criteria clearly defined
- [ ] Rollback/failure handling specified

If unsure whether to include something: **INCLUDE IT**. Over-specification is safe. Under-specification causes unattended failures.

Task instructions should emphasize:
- Persistence to complete the goal despite obstacles
- Task success is the measure, not partial progress
- All needed context is provided in this task file

**Why this matters:** Complete task authoring prevents unattended failures and prevents sub-agents from spawning research/explore agents, which would balloon token usage across the orchestration loop.

## Task Template

```markdown
# Task: [Descriptive Name]

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** task-name.md (or "None")

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

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
