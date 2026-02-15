---
name: pipeline
description: Pipeline commands for managing goals via ralph-plans API
---

# Pipeline

Continuous development pipeline. Goals are executable units of work, managed via the ralph-plans API service.

All scripts return JSON (`{"ok": true/false, ...}`).

## Flow

```
Goal (created via API) → Queue → Ralph executes → PR merges
```

## Default Workflow: Goals-First

**The goals-first workflow is the default for all work.** Local changes are rare exceptions that require explicit user instruction.

**Standard workflow:**

1. **Discuss** — User and Claude discuss the change and approach
2. **Create goal** — Claude creates the goal with clear acceptance criteria
3. **Queue immediately** — Goal is queued right after creation (default behavior)
4. **Ralph executes** — Ralph service picks up and executes the goal autonomously
5. **PR merges** — Completed work is merged via PR

**Default behaviors:**

- **Always queue after creation** — No manual testing or "trying it first" unless user explicitly requests it
- **No local changes** — Claude does not make local changes directly; work goes through Ralph

**When to make local changes (exceptions only):**

- User explicitly requests direct changes: "make this change now", "edit this file", "fix this directly"
- User explicitly says: "don't create a goal for this", "do this locally", "make this change here"

**If unsure:** Default to creating and queuing a goal.

## Goal Statuses

`draft` → `queued` → `running` → `done` (or `stuck` or `cancelled`)

## Goal Commands

Scripts live in `scripts/goal-*/run` with symlinks in `scripts/bin/`.

Flags `--org` and `--repo` are required on every call. Set `$RALPH_ORG` and `$RALPH_REPO` in `.envrc` for convenience.

| Command | Usage | Does |
|---------|-------|------|
| `goal-create` | `--org ORG --repo REPO --title "..." [--model MODEL] [--reasoning LEVEL] < body.md` | Create goal (draft) |
| `goal-list` | `[--status STATUS] [--org ORG] [--repo REPO]` | List goals, optionally filtered |
| `goal-get` | `<id>` | Read goal body + status |
| `goal-queue` | `<id>` | Transition draft → queued |
| `goal-start` | `<id>` | Mark goal as running |
| `goal-done` | `<id>` | Mark goal as done |
| `goal-stuck` | `<id>` | Mark goal as stuck |
| `goal-retry` | `<id>` | Retry a stuck goal |
| `goal-cancel` | `<id>` | Cancel a goal |
| `goal-comment` | `<id> < comment.md` | Add comment to goal |
| `goal-comments` | `<id>` | List comments on goal |

## Creating a Goal

```bash
echo "## Objective
Implement feature X per project/plan/feature-x.md.

## Reference
- project/plan/feature-x.md

## Outcomes
- Feature X working
- Tests pass

## Acceptance
- All quality checks pass" | goal-create --org "$RALPH_ORG" --repo "$RALPH_REPO" --title "Implement feature X"
```

**Optional flags:**

- `--model MODEL` — Override default model (haiku, sonnet, opus). Use for tasks requiring higher capability.
- `--reasoning LEVEL` — Set reasoning level (none, low, med, high). Use for complex architectural or debugging work.

```bash
# Complex architectural work
echo "..." | goal-create --org "$RALPH_ORG" --repo "$RALPH_REPO" --title "Redesign module X" --model opus --reasoning high
```

Then queue immediately:

```bash
goal-queue <id>
```

## Environment

Set in `.envrc`:

```bash
export RALPH_PLANS_HOST="localhost"
export RALPH_PLANS_PORT="5001"
export RALPH_ORG="mgreenly"
export RALPH_REPO="ikigai"
PATH_add scripts/bin
```

## Key Rules

- **Body via stdin** — `goal-create` reads body from stdin
- **--org/--repo on create** — `goal-create` requires `--org`/`--repo`; other commands use goal ID
- Goals can declare `Depends: #N, #M` in body; service waits for dependencies
