---
name: the-ralphs
description: Overview of the Ralph services and how they fit together in an Ikigai context
---

# The Ralphs

Ralph is a small ecosystem of services that together form an autonomous software development pipeline.

This skill is conceptual context for Ikigai agents. It does not replace the real services or scripts.

## Main Services

**ralph-plans** — Goal storage and lifecycle management. Tracks goals through states like `draft`, `queued`, `running`, `done`, `stuck`, and `cancelled`.

**ralph-runs** — Orchestrator. Picks up queued goals, creates isolated clones, runs agents, and merges results back.

**ralph-shows** — Dashboard for viewing queued, running, stuck, and completed goals.

**ralph-logs** — Browser log tailing for runs and related services.

**ralph-counts** — Metrics and cost tracking.

**ralph-remembers** — Long-term memory service, still evolving.

**ralph-herds** — Planned process supervisor for the whole stack.

## How They Fit Together

Typical flow:

1. A goal is created in `ralph-plans`
2. The goal is queued
3. `ralph-runs` picks it up
4. Work runs in an isolated clone
5. Results are merged back through the Ralph pipeline

## Three-Tier Git Model

1. Bare repo at `/mnt/store/git/<org>/<repo>`
2. GitHub as remote mirror
3. Working clones for humans and disposable clones for agents

## Important Operating Model

- Goals define **what**, not **how**
- Scripts are thin CLI wrappers around Ralph services
- Script JSON output is authoritative
- Agents should prefer goals-first workflow unless the user explicitly asks for local edits

## Real Script Location

When you need to act, use the real scripts in:

- `../ralph-pipeline/scripts` from the ikigai repo root
