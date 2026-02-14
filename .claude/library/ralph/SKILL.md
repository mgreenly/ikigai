---
name: ralph
description: Ralph - external goal execution service
---

# Ralph

Ralph is an external service that executes pipeline goals. It runs in the ralph-runs service, not locally.

## How It Works

1. Goals are created and queued via the ralph-plans API (`goal-create`, `goal-queue`)
2. The ralph-runs service picks up queued goals
3. Ralph clones the repo, executes the goal iteratively, commits progress
4. On completion, Ralph creates a PR and marks the goal as done

## Goal Authoring

Use `/load goal-authoring` for guidance on writing effective goal files. Key principles:

- Specify **WHAT**, never **HOW**
- Reference all relevant docs
- Include measurable acceptance criteria
- Trust Ralph to iterate and discover the path

## No Local Execution

Ralph does NOT run locally. Never execute ralph directly. The user manages the ralph-runs service separately.
