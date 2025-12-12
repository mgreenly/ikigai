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
    {"task": "shared-ctx-struct.md", "group": "Shared Context DI", "model": "sonnet", "thinking": "thinking"},
    {"task": "shared-ctx-cfg.md", "group": "Shared Context DI", "model": "sonnet", "thinking": "thinking"}
  ],
  "done": []
}
```

**Fields:**
- `task` - Filename (relative to tasks/ directory)
- `group` - Logical grouping for reporting
- `model` - Agent model: haiku, sonnet, opus
- `thinking` - Thinking level: none, thinking, extended, ultrathink

Execute strictly in order. Scripts manage todo→done movement. On failure with progress, `escalate.ts` updates model/thinking in place.

## Escalation Ladder

When a sub-agent fails but makes progress, the orchestrator escalates to higher capability:

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

**Escalation rules:**
- Failure → escalate and retry automatically
- Max level + failure → abort (human review needed)

## Task/Fix File Format

Each file specifies:
- **Target** - Which user story it supports
- **Agent model** - (now in order.json, but task file can specify for reference)
- **Pre-read sections** - Skills, docs, source patterns, test patterns
- **Pre-conditions** - What must be true before starting (implicit: clean working tree)
- **Task** - One clear, testable goal
- **TDD Cycle** - Red (failing test), Green (minimal impl), Refactor (clean up)
- **Post-conditions** - What must be true after completion

**Required Skill:** All task files MUST include `scm` in their Pre-read Skills section. This ensures agents commit after every testable change and never lose work.

**Implicit Pre-condition (ALL tasks):** Working tree must be clean (`git status --porcelain` returns empty). If uncommitted changes exist, abort immediately without making any changes.

## Scripts

Located in `.ikigai/scripts/tasks/`:

| Script | Purpose |
|--------|---------|
| `next.ts` | Get next task from order.json |
| `done.ts` | Mark task done in order.json |
| `escalate.ts` | Bump model/thinking to next escalation level |
| `session.ts` | Log timing events (start/done/retry), return elapsed time |

See `.ikigai/scripts/tasks/README.md` for full API documentation.

## Creating Tasks

When creating a new task file:

1. Write the task file to the appropriate `tasks/` or `fixes/` directory
2. Add entry to `order.json` (in `todo` array at appropriate position)
3. **Commit immediately** - task files must be committed before orchestration:
   ```bash
   git add tasks/new-task.md tasks/order.json
   git commit -m "chore: add <task-name> task"
   ```

This ensures orchestration starts with a clean working tree.

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
- Parses sub-agent JSON response
- On success: `session.ts done`, `done.ts`, report progress, loop
- On failure: `escalate.ts`, `session.ts retry`, loop with higher capability
- On max level failure: `session.ts done`, report max-level failure, stop
- Reports progress: `✓ task.md [12m 15s] | Total: 25m 8s | Remaining: 52`

**Critical:** Orchestrator never reads task files or runs make. Sub-agents do all work.

## Sub-agent Responsibilities

1. **FIRST: Verify clean working tree** - Run `git status --porcelain`. If ANY output exists, abort immediately with `{"ok": false, "reason": "Uncommitted changes in working tree"}`. Do not proceed.
2. Read their own task file (100% self-contained)
3. Verify pre-conditions
4. Execute TDD cycle
5. Verify post-conditions
6. **LAST: Leave working tree clean** - Either commit all changes OR roll back all changes. Never leave uncommitted files.
7. Return JSON response

**Critical: Clean Exit Requirement**

Sub-agents MUST leave the working tree clean before returning, regardless of success or failure:

- **On success:** Commit all changes with descriptive message
- **On failure:** Either commit partial progress (if useful) OR `git checkout .` to discard all changes
- **NEVER** return `{"ok": false, ...}` with uncommitted changes
- Run `git status --porcelain` before returning - if not empty, you haven't finished cleanup

This ensures the next task (or retry) starts with a clean slate.

**Response format:**
```json
{"ok": true}
{"ok": false, "reason": "..."}
```

## Rules

- Semantic filenames (not numbered)
- Sub-agents start with blank context - list all pre-reads in task file
- Pre-conditions of task N = post-conditions of task N-1
- One task/fix per file, one clear goal
- Always verify `make check` at end of each TDD phase
