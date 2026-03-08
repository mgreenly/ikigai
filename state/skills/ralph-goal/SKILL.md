---
name: ralph-goal
description: Create and manage Ralph goals from Ikigai using the real ralph-pipeline scripts
---

# Ralph Goals

Ralph is an autonomous development pipeline. Goals are executable units of work. Ralph picks up queued goals, executes them in isolated clones, and moves them through their lifecycle. All goal scripts return JSON like `{"ok": true/false, ...}`.

This Ikigai skill is the guidance layer. It references the real scripts in the sibling `ralph-pipeline` checkout rather than duplicating them.

## Script Location

Use the real scripts from `../ralph-pipeline/scripts` relative to the ikigai repo root.

Useful relative paths:

- **From ikigai repo root:** `../ralph-pipeline/scripts`
- **From this skill directory (`share/skills/ralph-goal`):** `../../../../ralph-pipeline/scripts`

Examples:

```bash
../ralph-pipeline/scripts/goal-list --status queued
../ralph-pipeline/scripts/goal-get 42
```

## Required Environment

The goal scripts require:

```bash
export RALPH_PLANS_HOST=localhost
export RALPH_PLANS_PORT=5001
```

## Default Workflow: Goals First

Goals-first is the default workflow.

Standard flow:

1. Discuss the change
2. Create a goal
3. Queue it immediately unless the user says otherwise
4. Ralph executes it

Local edits are the exception. Prefer goals unless the user explicitly asks for direct local changes.

## Goal Lifecycle

`draft` → `queued` → `running` → `done`

Other states:

- `stuck`
- `cancelled`

## Command Reference

All commands below live in `../ralph-pipeline/scripts/`.

| Command | Usage | Does |
|---|---|---|
| `goal-create` | `--title "..." [--org ORG] [--repo REPO] [--model MODEL] [--reasoning LEVEL] < body.md` | Create a draft goal. Body via stdin. |
| `goal-list` | `[--status STATUS] [--org ORG] [--repo REPO]` | List goals. |
| `goal-get` | `<id>` | Read one goal. |
| `goal-queue` | `<id>` | Move draft → queued. |
| `goal-cancel` | `<id>` | Cancel a non-terminal goal. |
| `goal-comment` | `<id> < comment.md` | Append a comment from stdin. |
| `goal-comments` | `<id>` | List comments. |
| `goal-done` | `<id>` | Move running → done. |
| `goal-retry` | `<id>` | Move stuck → queued. |
| `goal-start` | `<id>` | Move queued → running. |
| `goal-stuck` | `<id>` | Move running → stuck. |
| `goal-abort` | `<id>` | Abort a running goal by moving it to stuck. |
| `goal-depend` | `add <id> <dep_id>` / `remove <id> <dep_id>` / `list <id>` | Manage dependencies. |
| `goal-attachment-create` | `<goal_id> --name "name.md" < body.md` | Create attachment from stdin. |
| `goal-attachments` | `<goal_id>` | List attachments. |
| `goal-attachment-get` | `<goal_id> <attachment_id>` | Read one attachment. |
| `goal-attachment-edit` | `<goal_id> <attachment_id> --old-str "..." --new-str "..."` or `--body "..."` or `--body -` | Edit attachment body. |
| `goal-attachment-delete` | `<goal_id> <attachment_id>` | Delete attachment. |

## org and repo Rules

`goal-create` auto-derives `org` and `repo` from `git remote get-url origin` when `--org` and `--repo` are omitted.

Rules:

- Do not guess org or repo
- Omit `--org` and `--repo` for the current repo
- Only pass them when intentionally targeting another repo
- If you need explicit values, read them from that repo's remote first

If origin cannot be read, the script fails. Do not invent placeholder values.

## Creating a Goal

```bash
cat <<'EOF' | ../ralph-pipeline/scripts/goal-create --title "Add feature X"
## Objective
What should be accomplished.

## Reference
Relevant files, docs, and examples.

## Outcomes
Measurable, verifiable results.

## Acceptance
Success criteria.
EOF
```

Then queue it:

```bash
../ralph-pipeline/scripts/goal-queue <id>
```

## Attachments

Attachments are markdown files associated with a goal.

Examples:

```bash
# Create
cat notes.md | ../ralph-pipeline/scripts/goal-attachment-create 42 --name "notes.md"

# List
../ralph-pipeline/scripts/goal-attachments 42

# Get
../ralph-pipeline/scripts/goal-attachment-get 42 7

# Replace substring
../ralph-pipeline/scripts/goal-attachment-edit 42 7 --old-str "old text" --new-str "new text"

# Full replace from stdin
cat updated.md | ../ralph-pipeline/scripts/goal-attachment-edit 42 7 --body -
```

## Operational Guidance

- Prefer creating and queueing goals over making local changes
- Queue immediately by default
- Treat script JSON as the source of truth
- If a script returns `ok: false`, surface the error instead of guessing
- Use the companion `ralph-goal-authoring` skill when drafting the goal body
