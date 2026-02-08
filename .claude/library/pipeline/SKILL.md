---
name: pipeline
description: Pipeline commands for managing stories and goals
---

# Pipeline

Continuous development pipeline. Stories describe features, goals are executable units of work. All scripts return JSON (`{"ok": true/false, ...}`).

## Flow

```
Story (human writes) → Goals (decomposed) → Queue → Ralph executes → PR merges
```

## Goal Statuses

`draft` → `queued` → `running` → `spot-check` or `done` (or `stuck`)

## Story Commands

| Command | Usage | Does |
|---------|-------|------|
| `story-create` | `--title "..." < body.md` | Create story |
| `story-list` | `[--state open\|closed\|all]` | List stories |
| `story-get` | `<number>` | Read story + linked goals |

## Goal Commands

| Command | Usage | Does |
|---------|-------|------|
| `goal-create` | `--story <N> --title "..." [--spot-check] [--depends "N,M"] < body.md` | Create goal (draft) |
| `goal-list` | `[status]` | List goals, optionally by status |
| `goal-get` | `<number>` | Read goal body + status |
| `goal-queue` | `<number>` | Transition draft → queued |
| `goal-approve` | `<number>` | Approve spot-check: create PR, clean up clone |
| `goal-spot-check` | `<number> approve\|reject [--feedback "..."]` | Approve/reject after smoke test |

## Invocation

Scripts live in `.claude/harness/<name>/run` with symlinks in `.claude/scripts/`:

```bash
.claude/scripts/goal-list queued
.claude/scripts/goal-get 42
echo "## Objective\n..." | .claude/scripts/goal-create --story 15 --title "Add X"
```

## Logs

- **Orchestrator log**: `.pipeline/cache/orchestrator.log` — truncated on each orchestrator start
- **Ralph logs**: `.ralphs/<number>/ralph.log` — per-goal execution log in each clone directory

## Key Rules

- **Body via stdin** -- `goal-create` and `story-create` read body from stdin
- Goals reference parent story via `Story: #<number>` in body
- **Dependencies** -- Goals can declare `Depends: #N, #M` in body; orchestrator waits for dependencies to reach `goal:done` before picking up the goal
- **Story auto-close** -- When all goals for a story reach `goal:done`, the story is automatically closed
