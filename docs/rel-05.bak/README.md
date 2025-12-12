# Release Directory Structure

This directory contains task-based implementation work organized for TDD development with sub-agent orchestration.

## Directory Organization

```
.
├── README.md           # This file - explains directory structure
├── run.md             # Supervisor orchestration instructions
└── tasks/             # Implementation tasks
    ├── order.md       # Task execution order (strict top-to-bottom)
    └── *.md           # Individual task files
```

## Task Files

Each task file specifies:
- **Target** - Which user story or feature it supports
- **Agent model** - haiku (simple), sonnet (moderate), opus (complex)
- **Pre-read sections** - Skills, docs, source patterns, test patterns
- **Pre-conditions** - What must be true before starting
- **Task** - One clear, testable goal
- **TDD Cycle** - Red (failing test), Green (minimal impl), Refactor (clean up)
- **Post-conditions** - What must be true after completion

## Workflow

1. **Orchestrator** (run.md) supervises sub-agents executing tasks
2. **Sub-agents** work sequentially through order.md (top-to-bottom)
3. **Tasks** are smallest testable units of work
4. **Verification** after each task (make lint && make check)
5. **Commit** after successful completion

## Order Tracking

The `tasks/order.md` file lists all tasks organized by story or feature area. Completed items are marked with strikethrough (`~~item-name.md~~`).

Execution is strictly top-to-bottom. Never skip ahead.

## Rules

- Semantic filenames (not numbered)
- Sub-agents start with blank context - list all pre-reads
- Pre-conditions of task N = post-conditions of task N-1
- One task per file, one clear goal
- Always verify `make check` at end of each TDD phase
