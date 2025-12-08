# Tasks

Feature/release folder structure and workflow for TDD-based implementation.

## Folder Structure

Feature/release folders follow this structure:

- **README.md** - What this feature/release is about
- **user-stories/** - Requirements as numbered story files (01-name.md, 02-name.md)
- **tasks/** - Implementation work derived from stories
  - **order.json** - Task queue with model/thinking specification
  - **session.json** - Timing log (auto-generated)
- **fixes/** - Issues found during review, works like tasks/

## User Story Format

Each story file contains: Description, Transcript (example interaction), Walkthrough (numbered internal steps), Reference (API contracts as JSON).

## order.json

Located in `tasks/order.json` (and `fixes/order.json`):

```json
{
  "todo": [
    {"task": "shared-ctx-struct.md", "group": "Shared Context DI", "model": "sonnet", "thinking": "none"},
    {"task": "shared-ctx-cfg.md", "group": "Shared Context DI", "model": "sonnet", "thinking": "none"}
  ],
  "done": []
}
```

**Fields:**
- `task` - Filename (relative to tasks/ directory)
- `group` - Logical grouping for reporting
- `model` - Agent model: haiku, sonnet, opus
- `thinking` - Thinking level: none, thinking, extended, ultrathink

Execute strictly in order. Scripts manage todo→done movement.

## Task/Fix File Format

Each file specifies:
- **Target** - Which user story it supports
- **Agent model** - (now in order.json, but task file can specify for reference)
- **Pre-read sections** - Skills, docs, source patterns, test patterns
- **Pre-conditions** - What must be true before starting
- **Task** - One clear, testable goal
- **TDD Cycle** - Red (failing test), Green (minimal impl), Refactor (clean up)
- **Post-conditions** - What must be true after completion

## Scripts

Located in `scripts/tasks/`:

| Script | Purpose |
|--------|---------|
| `next.ts` | Get next task from order.json |
| `done.ts` | Mark task done in order.json |
| `session.ts` | Log timing events, return elapsed time |

See `scripts/tasks/README.md` for full API documentation.

## Workflow

1. **User stories** define requirements with example transcripts
2. **Tasks** are created from stories (smallest testable units)
3. **Orchestrator** (`/orchestrate PATH`) supervises sub-agents
4. **Review** identifies issues after tasks complete
5. **Fixes** capture rework from review process

## Orchestrator Role

Use `/orchestrate docs/rel-##/tasks` to start orchestration.

The orchestrator:
- Calls `next.ts` to get next task (with model/thinking)
- Calls `session.ts start` to log timing
- Spawns sub-agent with specified model/thinking
- Parses sub-agent JSON response (`{"ok": true}` or `{"ok": false, "reason": "..."}`)
- Calls `session.ts done` to complete timing
- Calls `done.ts` to move task to done array
- Reports progress: `task.md [12m 15s] | Total: 25m 8s | Remaining: 52`
- Loops until todo is empty or failure occurs

**Critical:** Orchestrator never reads task files or runs make. Sub-agents do all work.

## Sub-agent Responsibilities

1. Read their own task file (100% self-contained)
2. Verify pre-conditions
3. Execute TDD cycle
4. Verify post-conditions
5. Commit their own changes
6. Return JSON response: `{"ok": true}` or `{"ok": false, "reason": "..."}`

## Rules

- Semantic filenames (not numbered)
- Sub-agents start with blank context - list all pre-reads in task file
- Pre-conditions of task N = post-conditions of task N-1
- One task/fix per file, one clear goal
- Always verify `make check` at end of each TDD phase
