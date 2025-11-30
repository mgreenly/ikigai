# Tasks

## Description

Format specification for writing implementation tasks that follow TDD methodology.

## Overview

Tasks are the smallest independently testable units of work. Each task is handed to a sub-agent with a blank context. Tasks run sequentially and define clear pre/post conditions.

## Directory Structure

Tasks live in a `tasks/` subdirectory relative to the working tree root:

```
tasks/
  order.md              # manifest - defines execution sequence
  glob-struct.md        # semantic names, not numbered
  glob-registry.md
  glob-execute.md
  ...
```

## Manifest Format

`order.md` organizes tasks by user story, with completed tasks marked:

```markdown
# Task Order

## Story 01: Feature Name

- ~~completed-task.md~~ ✅
- ~~another-done.md~~ ✅
- next-pending-task.md
- future-task.md

## Story 02: Another Feature

- first-task.md
- second-task.md
```

Tasks execute strictly top-to-bottom. Reordering = edit this file, not rename task files.

## Task File Format

### Header

```markdown
# Task: [Short Descriptive Name]

## Target
User story: [which user story this supports, e.g., "02-single-glob-call"]

## Agent
model: [sonnet|haiku|opus]
```

Use `haiku` for straightforward tasks, `sonnet` for moderate complexity, `opus` for complex architectural work.

### Pre-read Sections

Sub-agents start with blank context. List everything they need:

```markdown
### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/naming.md
- docs/architecture.md

### Pre-read Source (patterns)
- src/openai/client.c (serialize_request implementation)
- src/error.h (error handling patterns)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (JSON test patterns)
```

Keep context minimal - only what's needed for this specific task.

### Pre-conditions

What must be true before this task runs:

```markdown
## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists in `src/tools/tool.h`
- Task `previous-task.md` completed successfully
```

### Task Description

One clear, testable goal:

```markdown
## Task
Implement `ik_tool_registry_t` that stores available tools and provides lookup by name.
```

### TDD Cycle

Always follow Red → Green → Refactor:

```markdown
## TDD Cycle

### Red
1. Create `tests/unit/test_tool_registry.c`
2. Write test: create registry, add tool, lookup by name
3. Run `make check` - expect compile failure (types don't exist)

### Green
1. Create `src/tools/registry.h` with `ik_tool_registry_t`
2. Implement `ik_tool_registry_create()`, `_add()`, `_lookup()`
3. Run `make check` - expect pass

### Refactor
1. Check for duplication with existing registry patterns
2. Ensure naming matches docs/naming.md
3. Run `make check` - verify still green
```

### Post-conditions

What must be true after this task completes:

```markdown
## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_registry_t` exists with create/add/lookup functions
- 100% test coverage for new code
```

## Supervisor Workflow

A supervisor agent coordinates task execution:

1. Read the next incomplete task from `order.md`
2. Launch sub-agent with the model specified in the task file
3. Wait for sub-agent to complete
4. Launch verification sub-agent to confirm post-conditions
5. If verification passes:
   - Mark task done in order.md: `- ~~task-name.md~~ ✅`
   - Commit changes
   - Move to next task
6. If verification fails:
   - Re-run sub-agent if it made progress
   - If stuck, report to user and wait

The supervisor does NOT run code or tests directly - sub-agents do all implementation work.

## Rules

- One task per file
- Semantic filenames (not numbered)
- Smallest independently testable unit
- Each task must be completable with blank context + listed files
- Pre-conditions of task N should match post-conditions of task N-1
- Always verify `make check` at end of each TDD phase
