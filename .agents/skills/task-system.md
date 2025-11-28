# Task System

## Description
Expert knowledge of the hierarchical task system for breaking down complex work into executable steps with automated sub-agent orchestration.

## Task System Architecture

The task system enables breaking down complex work into hierarchical task files that can be executed manually or automatically by sub-agents.

### Task Components

**Task directory structure:**
```
.tasks/                      # Default task directory
├── state.json               # Execution state tracking
├── README.md                # Task series overview
├── 1-first-task.md          # Task files with decimal numbering
├── 1.1-subtask.md
└── 2-second-task.md
```

**State file (`state.json`):**
```json
{
  "current": "2",            // Current task being worked on (or null)
  "status": "pending"        // pending | in_progress | done
}
```

**State transitions:**
- `pending` → `in_progress` (agent starts task)
- `in_progress` → `done` (agent completes task)
- `done` → `pending` (human verifies, advances to next)

### Task File Structure

Every task file follows this markdown structure:

```markdown
# Task: [Title]

## Description
[Exactly 2 sentences - what and why]

## Context
[Overall context, relevant files, integration points]

## Prerequisites
- [Required prior tasks or environment]
- [Or "None"]

## Steps

### 1. [Step Name]
**Documentation:**
- Read `.agents/skills/default.md`
- [Other relevant docs]

**Context:**
- [Step-specific context]

**Execute:**
- [Specific actions]

**Success Criteria:**
- [Measurable outcomes]

### 2. [Next Step]
[Same structure...]

## Outputs
- `name`: Description of output for later tasks
- [Or "None"]

## Post-Work
- **Action**: [wait | notify | continue]
- **Message**: [What to communicate]

## On Failure
- **Retry**: [yes | no]
- **Block subsequent tasks**: [yes | no]
- **Fallback**: [Alternative or "None"]
```

### Task Numbering System

Hierarchical decimal numbering enables unlimited depth and natural sorting:

- Format: `{number}-{name}.md`
- Examples: `1-setup.md`, `1.1-database.md`, `1.1.1-schema.md`
- Sort order: `1 → 1.1 → 1.1.1 → 1.1.2 → 1.2 → 1.10 → 2`
- Parent before children (depth-first traversal)
- Full decimal comparison (1.9 < 1.10, not "1.9" vs "1.1")

### Task Workflows

**Manual workflow (human verification):**
1. `/next-task` - Execute next task
2. Agent marks done, waits
3. `/verify-task` - Human approves, advances
4. Repeat

**Automated workflow (sub-agent execution):**
1. `/auto-tasks` - Start automation
2. Supervisor gets next task
3. Spawns task sub-agent (`execute-task-auto.md`)
4. Task sub-agent spawns step sub-agents (one per step)
5. Task sub-agent returns JSON report
6. If success: verify and continue
7. If failure: STOP and report to user

### Sub-Agent Hierarchy

**Task creation (parallel):**
```
Main Agent (create-tasks.md)
  ├─> Identifies 5-10 phases
  └─> Spawns N sub-agents IN PARALLEL (one per phase)
      └─> Each writes complete task file
```

**Task execution (automated):**
```
Supervisor (auto-tasks.md)
  └─> For each task:
      └─> Spawns task executor (execute-task-auto.md)
          └─> For each step:
              ├─> Spawns step executor (Haiku or Sonnet)
              └─> Verifies success criteria
          └─> Returns JSON report
```

**Model selection:**
- **Haiku**: Simple verification (run make, check exit codes, parse output)
- **Sonnet**: Complex work (write code, refactor, debug, analyze)

### Standard Task Template: TDD Workflow

For code development tasks, use the standard TDD workflow with clear Sonnet/Haiku delegation:

1. **Write Failing Tests** (Sonnet) - Create comprehensive test cases
2. **Verify Tests Fail** (Haiku) - Run `make check`, confirm expected failures
3. **Implement Code** (Sonnet) - Write implementation to pass tests
4. **Verify Tests Pass** (Haiku) - Run `make check`, confirm all pass (loop with steps 3-4 max 3 times if failures)
5. **Refactor** (Sonnet) - Eliminate redundancy, improve clarity
6. **Verify Refactor** (Haiku) - Run `make check`, confirm tests still pass
7. **Achieve Coverage** (Sonnet) - Add tests until 100% coverage
8. **Verify Coverage** (Haiku) - Run `make coverage`, confirm 100%
9. **Check Lint** (Haiku) - Run `make lint`, report any issues
10. **Fix Lint Issues** (Sonnet) - Address lint failures (if any from step 9)
11. **Re-verify Coverage** (Haiku) - Run `make coverage`, confirm still 100%
12. **Fix Coverage Gaps** (Sonnet) - Add tests if coverage dropped (if needed from step 11)
13. **Sanitizer Validation** (Haiku) - Run `make BUILD=sanitize check && make BUILD=tsan check`, verify clean
14. **Draft Commit Message** (Sonnet) - Create clear commit message (no attribution lines)
15. **Create Commit** (Haiku) - Stage files and commit with message from step 14

Each step includes Documentation/Context/Execute/Success Criteria sections.

## Common Patterns

### Script Invocation
```bash
deno run --allow-read .agents/scripts/task-list/run.ts .tasks
deno run --allow-read --allow-write .agents/scripts/task-next/run.ts .tasks
deno run --allow-read --allow-write .agents/scripts/task-start/run.ts .tasks <number>
deno run --allow-read --allow-write .agents/scripts/task-done/run.ts .tasks
deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts .tasks
```

### Spawning Task Sub-Agents
```markdown
Use Task tool with subagent_type="general-purpose"
Prompt: Read `.agents/skills/execute-task-auto.md` and execute task N
Wait for JSON response with success/failure and details
```

### Creating Task Files
```markdown
Use Task tool with subagent_type="general-purpose"
Prompt: Read `.agents/skills/create-tasks.md` and create task file for phase X
Spawn multiple sub-agents IN PARALLEL for different phases
```

## Task Scripts

The task system includes several Deno scripts for state management:

- **task-list**: List and sort all tasks in directory
- **task-next**: Get next task based on current state
- **task-start**: Mark specific task as in_progress
- **task-done**: Mark current task as done
- **task-verify**: Verify current task and advance to next

All scripts return JSON: `{success: bool, data?: {...}, error?: string, code?: string}`

See each script's `README.md` for detailed usage and arguments.

## Troubleshooting

### Agent won't advance
- Check `state.json` status
- If "done", run `/verify-task`
- Verify task numbering is sequential

### Wrong task executing
- Check `state.json` current
- Verify task files sorted correctly
- Use `task-list` script to debug order

### Task fails but marked done
- Don't verify - fix the issue first
- Reset state: `task-start <number>`
- Re-run the task

### Sub-agent returns invalid JSON
- Check execute-task-auto.md prompt clarity
- Verify task file structure (Documentation/Context/Execute/Success Criteria)
- Ensure success criteria are measurable

## Related Documentation

- `docs/task-system.md` - Task system architecture reference
- `docs/task-system-guide.md` - Task system usage guide with examples
- `docs/agent-scripts.md` - Script architecture details
- `.agents/skills/create-tasks.md` - Task creation guide
- `.agents/skills/execute-next-task.md` - Manual task execution
- `.agents/skills/execute-task-auto.md` - Automated task execution

## Summary

The task system provides:
- Hierarchical task breakdown with decimal numbering
- State management for tracking progress
- Manual execution with human verification
- Automated execution with sub-agent orchestration
- Standard TDD workflow templates for code development
- Scripts for task state manipulation

Use this system to break complex work into manageable, executable steps with clear success criteria.
