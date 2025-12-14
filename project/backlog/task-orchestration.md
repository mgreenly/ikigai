# Task Orchestration System

## Summary

Script-driven task orchestration with minimal orchestrator context usage, self-contained task files, and session timing.

## Motivation

- Orchestrator wastes context reading task files and order lists
- Keeping two files in sync (order.md + progress tracking) causes confusion
- No visibility into session duration or task timing
- Sub-agent handoff requires structured responses for reliable parsing

## Design

### Scripts

Located in `.agents/scripts/tasks/`:

| Script | Purpose |
|--------|---------|
| `next.ts` | Get next task from order.json |
| `done.ts` | Mark task done in order.json |
| `session.ts` | Log events, return elapsed work time |

### order.json

Located in `rel-##/tasks/order.json`:

```json
{
  "todo": [
    {"task": "shared-ctx-struct.md", "group": "Shared Context DI"},
    {"task": "shared-ctx-cfg.md", "group": "Shared Context DI"}
  ],
  "done": []
}
```

Two arrays only. Items processed strictly in order. Group is metadata for reporting.

### session.json

Located in `rel-##/tasks/session.json`:

```json
[
  {"event": "start", "task": "shared-ctx-struct.md", "time": "2025-01-07T10:30:00Z"},
  {"event": "done", "task": "shared-ctx-struct.md", "time": "2025-01-07T10:42:15Z"},
  {"event": "start", "task": "shared-ctx-cfg.md", "time": "2025-01-07T10:42:20Z"},
  {"event": "done", "task": "shared-ctx-cfg.md", "time": "2025-01-07T10:55:33Z"}
]
```

Append-only event log. Both success and failure are recorded as `done` events.

### Script Interfaces

**next.ts**
```bash
deno run .agents/scripts/tasks/next.ts docs/rel-05/tasks/order.json
```
```json
{"success": true, "data": {"task": "shared-ctx-cfg.md", "group": "Shared Context DI"}}
```
or when complete:
```json
{"success": true, "data": null}
```

**done.ts**
```bash
deno run .agents/scripts/tasks/done.ts docs/rel-05/tasks/order.json shared-ctx-cfg.md
```
```json
{"success": true, "data": {"remaining": 52}}
```

**session.ts**
```bash
deno run .agents/scripts/tasks/session.ts docs/rel-05/tasks/session.json start shared-ctx-cfg.md
deno run .agents/scripts/tasks/session.ts docs/rel-05/tasks/session.json done shared-ctx-cfg.md
```
```json
{"success": true, "data": {"elapsed_seconds": 1508, "elapsed_human": "25m 8s"}}
```

### Orchestrator Command

New `/orchestrate` command. Behavior:

1. Call `next.ts` to get task name
2. Call `session.ts start <task>`
3. Spawn sub-agent: "Read and execute docs/rel-##/tasks/<task>"
4. Parse structured response from sub-agent
5. Call `session.ts done <task>` (success or failure)
6. If ok:
   - Call `done.ts <task>`
   - Report: `shared-ctx-cfg.md [12m 15s] | Total: 25m 8s | Remaining: 52`
   - Loop to step 1
7. If not ok:
   - Report failure reason
   - Stop, wait for human input

**Critical rules:**
- Orchestrator NEVER reads task files (sub-agents do)
- Orchestrator NEVER runs make commands (sub-agents do)
- Orchestrator only: spawns, parses responses, calls scripts, reports

### Sub-agent Response Format

Sub-agents return exactly:

```json
{"ok": true}
```
or
```json
{"ok": false, "reason": "brief explanation"}
```

### Sub-agent Responsibilities

1. Read their own task file (100% self-contained)
2. Verify pre-conditions (spawn haiku sub-sub-agent if needed)
3. Execute TDD cycle
4. Verify post-conditions
5. Commit their own changes
6. Return structured JSON response

If pre-conditions fail and cannot be fixed, return failure and stop.

### Task Files

Remain unchanged. All context for sub-agents lives in the task file:
- Pre-read skills, docs, source, tests
- Pre-conditions and post-conditions
- TDD cycle instructions
- Model specification

Task files can be optimized independently without affecting orchestrator.

## Rationale

- **Scripts over file parsing**: Orchestrator gets minimal JSON responses, saves context
- **Single order.json**: No sync issues between multiple files
- **todo/done arrays**: Binary state (done or not done), no "pending" vs "in-progress"
- **Session logging**: Visibility into work time without complex state
- **Self-contained tasks**: All optimization happens in task files, not orchestrator prompts
- **Sub-agent commits**: Saves orchestrator context, sub-agent has full diff context
