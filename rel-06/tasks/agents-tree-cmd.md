# Task: /agents Command

## Target
User Story: 19-agents-tree.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Agent Registry section)

## Pre-read Source (patterns)
- READ: src/commands.c
- READ: src/repl.h (agents array)
- READ: src/agent.h (parent_uuid field)

## Files to Create/Modify
- CREATE: tests/unit/commands/cmd_agents_test.c
- MODIFY: src/commands.c (add cmd_agents function and register command)
- READ from registry: src/db/agent.h, src/db/agent.c (registry query functions from registry-queries.md)

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- registry-queries.md complete (provides db/agent.h query functions)
- mail-filter-cmd.md complete

## Task
Implement `/agents` command to display agent hierarchy tree.

**Command:**
```
/agents
```

**Output:**
```
Agent Hierarchy:

* abc123... (running) - root
    xyz789... (running)
      child1... (running)
      child2... (dead)
    def456... (running)

4 running, 2 dead
```

## TDD Cycle

### Red
1. Create `tests/unit/commands/cmd_agents_test.c`:
   - Test displays tree structure
   - Test current agent marked with *
   - Test shows status (running/dead)
   - Test root labeled
   - Test indentation reflects depth
   - Test summary count correct

2. Run `make check` - expect test failures

### Green
1. Implement `cmd_agents()` in `src/commands.c`
2. Build tree from parent_uuid relationships
3. Recursive display with indentation
4. Register command
5. Run `make check` - expect pass

### Refactor
1. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/agents` displays hierarchy
- Current agent marked
- Working tree is clean (all changes committed)
