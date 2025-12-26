---
name: task-authoring
description: Task Authoring skill for the ikigai project
---

# Task Authoring

Creating task files from requirements (user stories, bugs, gaps).

## Critical: Unattended Execution

**No human in the loop.** When `/orchestrate` runs, tasks execute automatically. Missing context causes:
- Failed escalations (wasted attempts)
- Partial success (technical debt)
- Research sub-agents (token bloat)

**The task file IS the help. There is no one to ask.**

## Efficiency Principle

**Spend generously during authoring to save massively during execution.**

- **Authoring (once):** Research, explore, read extensively, think deeply
- **Execution (N times):** Sub-agent has everything, executes immediately

**Token math:**
- Poor: 10K authoring → 20 tasks research → 200K+ execution
- Good: 50K authoring → 20 tasks execute directly → 40K execution

## Process

1. **Review** - Understand requirements thoroughly
2. **Think** - Consider edge cases, dependencies, approaches
3. **Break down** - Smallest achievable, testable units
4. **Create order.json** - Define execution order and metadata

## Abstraction Levels

### scratch/plan/ - Module Level
- Module names and responsibilities
- Major structs (names/purpose only)
- Key function signatures (declarations)
- Data flow between modules

**Exclude:** Full struct definitions, implementation code, algorithms.

### scratch/tasks/ - Interface Level
- Function signatures with purpose docs
- Struct names with member descriptions
- Expected behaviors and contracts
- Test scenarios (descriptions only)

**Exclude:** Implementation code, complete structs, working test code.

## File Naming

**No numeric prefixes.** Order comes from `order.json`.

| Good | Bad |
|------|-----|
| `provider-types.md` | `01-provider-types.md` |
| `openai-adapter.md` | `02-openai-adapter.md` |

## order.json Structure

**Single source of truth** for task execution:

```json
{
  "todo": [
    {"task": "provider-types.md", "group": "Foundation", "model": "sonnet", "thinking": "none"},
    {"task": "openai-adapter.md", "group": "Migration", "model": "sonnet", "thinking": "thinking"}
  ]
}
```

| Field | Description |
|-------|-------------|
| `task` | Filename (must exist in same directory) |
| `group` | Logical grouping for reporting |
| `model` | `sonnet` or `opus` |
| `thinking` | `none`, `thinking`, or `extended` |

**Tasks execute serially in array order.** Position determines execution, not `Depends on:` field. Database tracks completion state.

## Task File Requirements

### Model Selection

Choose minimum capability needed:

| Complexity | Model | Thinking |
|------------|-------|----------|
| Straightforward | sonnet | none |
| Moderate | sonnet | thinking |
| Complex | sonnet | extended |
| Very complex | opus | extended |

Default to `sonnet/none`, escalate when needed.

### Working Directory Context

**Every task MUST state:**
- Working directory is project root (where `Makefile` lives)
- All paths relative to project root, not task file

### Skill Loading

- List specific skills needed for THIS task only
- Don't load large skills (database, source-code) unless directly required
- **Never load `align`** - sub-agents execute, don't negotiate
- **Goal:** Immediate execution, no exploration

### Source Code Links

**Provide ALL file paths needed - be complete:**
- Implementation files to read/modify
- Test files as patterns
- Related existing code as examples
- Sub-agent should NOT need to search

### Pre/Post Conditions

- Pre-conditions: verifiable before starting
- Post-conditions: testable after completion
- Chain: post(N) = pre(N+1)

### Context (What/How/Why)

- **What** - Specific goal with full details
- **How** - Exact approach, patterns, implementation guidance
- **Why** - Rationale for decision-making

### Execution Checklist

- [ ] All required skills listed
- [ ] All file paths to read/modify listed
- [ ] Test patterns and examples referenced
- [ ] Edge cases and error conditions documented
- [ ] Success criteria defined
- [ ] Rollback/failure handling specified

**When unsure: INCLUDE IT.** Over-specification is safe. Under-specification causes unattended failures.

## Task Template

```markdown
# Task: [Descriptive Name]

**UNATTENDED EXECUTION:** Complete context required.

**Model:** sonnet/thinking
**Depends on:** task-name.md (or "None")

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths relative to project root**, not this task file.

All context provided here. Do not research or spawn sub-agents.

## Pre-Read

**Skills:**
- `/load skill-name` - Why needed

**Source:**
- `path/to/file.h` - What to learn

**Plan:**
- `scratch/plan/relevant.md` - Sections to reference

## Objective

What this task accomplishes and why.

## Interface

| Function | Purpose |
|----------|---------|
| `res_t foo_create(...)` | Creates X, returns OK/ERR |
| `void foo_destroy(...)` | Cleanup, NULL-safe |

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
