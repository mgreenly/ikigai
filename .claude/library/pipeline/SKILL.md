---
name: pipeline
description: Pipeline commands for managing stories and goals
---

# Pipeline

Continuous development pipeline. Stories describe features, goals are executable units of work. All scripts return JSON (`{"ok": true/false, ...}`).

**Trial/Debug Mode:** Currently all goals use `--story 0` to disable story tracking. Goals are standalone during this phase.

## Flow

```
Story (human writes) → Goals (decomposed) → Queue → Ralph executes → PR merges
```

**Current:** Goals bypass story mechanism (`--story 0`) during trial/debug phase.

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
echo "## Objective\n..." | .claude/scripts/goal-create --story 0 --title "Add X"
```

**Note:** During trial/debug phase, always use `--story 0` when creating goals.

## Logs

- **Orchestrator log**: `.pipeline/cache/orchestrator.log` — truncated on each orchestrator start
- **Ralph logs**: `.ralphs/<org>/<repo>/<number>/.pipeline/cache/ralph.log` — per-goal execution log in each clone directory

## Goal Authoring

Goal bodies **must** follow the `goal-authoring` skill guidelines (`/load goal-authoring`). Key rules:

- Specify **WHAT**, never **HOW** — outcomes, not steps
- Reference relevant files — Ralph reads them across iterations
- Include measurable **acceptance criteria**
- Never pre-discover work (no specific line numbers or code snippets)
- Trust Ralph to iterate and discover the path

## Key Rules

- **Body via stdin** -- `goal-create` and `story-create` read body from stdin
- **Trial/debug mode** -- Use `--story 0` for all goals during trial/debug phase; stories are disabled
- Goals reference parent story via `Story: #<number>` in body (currently `Story: #0` for all goals)
- **Dependencies** -- Goals can declare `Depends: #N, #M` in body; orchestrator waits for dependencies to reach `goal:done` before picking up the goal
- **Story auto-close** -- When all goals for a story reach `goal:done`, the story is automatically closed (inactive during trial/debug phase)
