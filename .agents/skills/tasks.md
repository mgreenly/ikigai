# Tasks

## Description

Format specification for writing implementation tasks that follow TDD methodology.

## Overview

Tasks are the smallest independently testable units of work. Each task is handed to a sub-agent with a blank context. Tasks run sequentially and define clear pre/post conditions.

## Directory Structure

```
rel-04/
  tasks/
    order.md              # manifest - defines execution sequence
    glob-struct.md        # semantic names, not numbered
    glob-registry.md
    glob-execute.md
    ...
```

## Manifest Format

`order.md` is a simple list of filenames, one per line:

```markdown
# Task Order

glob-struct.md
glob-registry.md
glob-execute.md
request-serialize.md
```

Reordering = edit this file, not rename task files.

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

### Pre-conditions

What must be true before this task runs:

```markdown
## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists in `src/tools/tool.h`
- No existing glob implementation
```

### Context

Explicit list of files to read. Sub-agents start with blank context:

```markdown
## Context
Read before starting:
- docs/memory.md (ownership patterns)
- docs/return_values.md (Result types)
- src/tools/tool.h (existing types)
```

Keep context minimal - only what's needed for this specific task.

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

## Rules

- One task per file
- Semantic filenames (not numbered)
- Smallest independently testable unit
- Each task must be completable with blank context + listed files
- Pre-conditions of task N should match post-conditions of task N-1
- Always verify `make check` at end of each TDD phase
