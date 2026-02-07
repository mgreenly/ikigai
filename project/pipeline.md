# Claude Pipeline

Continuous development pipeline that turns user stories into shipped code with minimal human intervention.

## Problem

The human is the bottleneck in two places:

1. **Verification** — Reviewing every PR before merge doesn't scale to high volume.
2. **Planning** — Only the human can define top-level goals, and decomposing them into executable tasks is manual.

## Solution

Move verification from per-PR to per-release. Automate the decomposition of high-level intent into executable goals. Let automated checks gate merges. The human verifies the integrated result at release time.

## Terminology

### Artifacts

| Name | What it is |
|------|-----------|
| **Story** | Human-written speculative user documentation for a feature that doesn't exist yet |
| **Goal** | Executable unit of work derived from a story — what ralph receives |
| **Bible** | Repo docs that define how code should be written (the human's steering mechanism) |
| **PR** | Pull request produced by ralph |
| **Release** | Tagged version after human verification |

### Actors

| Name | What it does |
|------|-------------|
| **Human** | Writes stories, maintains the bible, spot-checks, verifies releases |
| **Claude** | Reads a story + bible + codebase, produces goals (decomposition) |
| **Ralph** | Executes a single goal in a fresh clone, produces a PR |
| **Orchestrator** | Dumb worker pool — pulls queued goals, runs ralphs, cleans up |

### Actions

| Name | Who | What |
|------|-----|------|
| **Decompose** | Claude | Break a story into goals |
| **Queue** | Human | Mark a goal as ready for execution |
| **Execute** | Ralph | Work a goal, run checks, prepare a PR |
| **Spot-check** | Human | Smoke test a finished goal before PR pushes |
| **Merge** | Automated | PR auto-merges when checks pass |
| **Re-queue** | Orchestrator/Human | Send a failed goal back for another attempt |
| **Release** | Human | Verify integrated product, tag it |
| **Unstick** | Human + Claude | Pull a stuck goal, diagnose, fix, re-introduce |

### Goal Statuses

| Status | Meaning |
|--------|---------|
| **Draft** | Created by Claude, not yet approved |
| **Queued** | Approved and waiting for execution |
| **Running** | Ralph is actively executing it |
| **Spot-check** | Ralph finished, waiting for human smoke test |
| **Done** | PR merged |
| **Stuck** | High retry count, needs human intervention |

## Pipeline Flow

```
Human writes story
    → Claude decomposes into goals
        → Human queues approved goals
            → Orchestrator runs ralphs in fresh clones
                → No spot-check: PR auto-merges if checks pass
                → Spot-check: Ralph pauses, human smoke tests
                    → Pass: PR proceeds
                    → Fail: Goal gets feedback, re-queues
                        → Human verifies at release time
```

## Backing Store — GitHub Issues

Stories and goals are GitHub Issues with label conventions. No external services, no database — just `gh` CLI calls.

### Label Scheme

| Label | Purpose |
|-------|---------|
| `pipeline` | All pipeline-managed issues (stories and goals) |
| `story` | Issue is a story |
| `goal` | Issue is a goal |
| `goal:draft` | Goal created, not yet approved |
| `goal:queued` | Goal approved, waiting for execution |
| `goal:running` | Ralph is executing this goal |
| `goal:spot-check` | Waiting for human smoke test |
| `goal:done` | PR merged |
| `goal:stuck` | High retry count, needs human intervention |
| `spot-check` | Goal requires human smoke test before PR |

### Conventions

- **Stories** reference goals via issue links in the body
- **Goals** reference their parent story via `Story: #<issue-number>` in the body
- **PRs** reference goals via `Closes #<issue-number>`
- **Status transitions** are label swaps (remove old status label, add new one)
- All pipeline issues carry the `pipeline` label for easy filtering
- The orchestrator queries for work: `gh issue list --label pipeline,goal:queued`

### Why GitHub Issues

- `gh` CLI makes it fully scriptable — no SDK, no extra API tokens
- PRs close issues natively
- Labels model the status lifecycle
- Issue templates can enforce the goal/story format
- Zero new dependencies

## Actors

### Human

- Writes stories (speculative user documentation / user journeys)
- Maintains the bible (project docs that define how code should be written)
- Queues approved goals for execution
- Spot-checks flagged goals before they push PRs (start app, quick smoke test)
- Monitors PR activity, pulls stuck goals (high retry count) for discussion
- Verifies the integrated product at release time, tags the release
- Refines the bible when agent output deviates from intent

### Claude

- Reads a story, the bible, and the codebase
- Produces a set of goals with dependency and metadata
- Triggered via `/decompose` slash command
- Resolves stuck goals in conversation with the human

### Ralph

- Executes a single goal in a fresh clone
- Produces a PR with all quality checks passing
- No awareness of the broader pipeline — just one goal at a time

### Orchestrator

Ruby script. Dumb worker pool — no scheduling intelligence, no dependency ordering. Run in the foreground with timestamped log output.

**Configuration (global, applies to all ralphs):**
- `--max N` — maximum concurrent ralphs
- Ralph flags (model, thinking level, etc.) passed through uniformly

**Main loop:**

```
while running:
  collect finished ralphs (non-blocking Process.wait2)
  for each completed ralph:
    if success and no spot-check:  push PR, goal → goal:done, delete clone
    if success and spot-check:     goal → goal:spot-check, notify human, keep clone
    if failure:                    goal → goal:queued (re-queue), delete clone
                                   if retry count > threshold: goal → goal:stuck
  while slots available AND queued goals exist:
    pick a goal (goal-list queued)
    goal → goal:running
    clone repo from remote to .ralphs/<number>/
    spawn ralph with output redirected to .ralphs/<number>/ralph.log
    track {pid, goal, dir, started}
  sleep 5 seconds
```

**Clone directory:**

```
.ralphs/                    # gitignored
├── 42/                     # clone for goal #42
│   ├── ralph.log           # stdout/stderr, tail -f to watch
│   └── ...                 # the cloned repo
├── 43/
│   ├── ralph.log
│   └── ...
```

Each ralph clones from the remote so it starts from clean `main@origin`. Clones live in `.ralphs/` in the project root — `ls .ralphs/` shows what's active.

**Cleanup rules:**
- No spot-check, success → delete clone immediately
- Spot-check → clone stays until `goal-spot-check approve|reject` cleans it up
- Failure → delete clone before re-queuing

**Ctrl+C shutdown:**
- Running ralphs → kill process, re-queue goal, delete clone
- Spot-check goals (already paused) → leave clone and label alone
- Shutdown takes seconds — orchestrator knows all state in memory, no GitHub queries needed

**Log output:**

```
[2026-02-07 14:23:01] Starting orchestrator (max 3 ralphs)
[2026-02-07 14:23:02] Goal #42 "Add widget rendering" → running (slot 1/3)
[2026-02-07 14:23:02] Goal #42 cloned to .ralphs/42/
[2026-02-07 14:23:02] Goal #42 ralph started (pid 12345, log: .ralphs/42/ralph.log)
[2026-02-07 14:25:31] Goal #42 ralph finished (exit 0, 2m29s)
[2026-02-07 14:25:31] Goal #42 spot-check required → goal:spot-check
[2026-02-07 14:30:12] Goal #43 ralph finished (exit 1, 5m07s)
[2026-02-07 14:30:12] Goal #43 failed → re-queued (retry 2/5)
[2026-02-07 14:30:12] Goal #43 cleaned up .ralphs/43/
```

**Ralph prerequisites:**
- Ralph needs a `--log-mode` flag that disables the spinner and terminal control codes, outputting plain text suitable for log files.

## Stories

GitHub Issues labeled `story`. They describe what the user sees and does — speculative user documentation for features that don't exist yet.

The human writes stories. Claude reads them during decomposition.

## Goals

GitHub Issues labeled `goal`. Each goal body includes:

- Standard ralph-goal sections (Objective, Reference, Outcomes, Acceptance)
- **Story reference** — `Story: #<issue-number>`
- **Retry count** — incremented each time the goal is re-queued after a failure
- **Spot-check** — `spot-check` label if the human must smoke test the result

## Bible

Markdown documents in the repo that define how code should be written — naming conventions, architectural patterns, style preferences, quality expectations. The human maintains these. Claude references them during decomposition to produce goals that align with the human's intent.

The bible is the human's primary steering mechanism. Rather than reviewing every PR, the human reviews the integrated product at release time and refines the bible when the output deviates from their vision. Future decomposition reflects the updated guidance.

## Spot Checks

Some goals are labeled `spot-check` — typically UI-facing changes, new features, or anything where "does it visibly work" matters more than what the code looks like. This is not code review. The human starts the app, spends 30 seconds confirming the feature works on the surface, and either approves or rejects.

When ralph finishes a spot-check goal, it pauses before pushing the PR and notifies the human. The human runs the app from ralph's working directory and does a quick smoke test:

- **Pass** — Human approves, ralph pushes the PR, auto-merge proceeds.
- **Fail** — Human provides brief feedback, goal re-queues with the feedback attached. Retry count increments.

The spot-check flag can be set by the human when approving goals from `/decompose`, or `/decompose` can suggest it based on heuristics (e.g., new user-facing features get flagged, internal refactors don't).

## Merge Conflicts

When a PR can't merge cleanly (because another PR landed first and changed the same area):

1. Delete the PR and branch
2. Re-queue the original goal
3. Ralph runs again against current main
4. The goal succeeds because the conflicting change is already in main

No intelligence needed. The conflict only existed because two ralphs worked against the same base.

## Stuck Goals

A goal is stuck when it accumulates a high retry count — it keeps failing or producing merge conflicts repeatedly. When the human notices this:

1. Pull the goal out of the queue (swap label to `goal:stuck`)
2. Discuss with Claude to diagnose and fix the goal
3. Re-introduce the corrected goal

## Release Cycle

Weekly cadence:

1. Slow down intake of new goals
2. Ensure all quality checks pass on main
3. Human verifies the integrated product (e.g., render all tools, test key user journeys)
4. Tag the release
5. Resume high-volume goal intake

This batches human verification. Instead of checking the same thing per-PR, the human checks it once per release across all merged changes.

## Scripts

Ruby scripts that wrap `gh` CLI calls and return structured JSON. Actors never compose `gh` commands directly — they call scripts and consume the output. This minimizes tokens burned by the orchestrator and planner.

### Layout

```
.claude/harness/           # Actual scripts (directories, can have sibling files)
├── goal-create/run        # gh issue create --label goal,goal:draft ...
├── goal-list/run          # gh issue list --label goal:<status> --json ...
├── goal-get/run           # gh issue view <number> --json ...
├── goal-queue/run         # Label swap: goal:draft → goal:queued
├── goal-spot-check/run    # Approve or reject: goal:spot-check → goal:done or re-queue
├── story-create/run       # gh issue create --label story ...
├── story-list/run         # gh issue list --label story --json ...
├── story-get/run          # gh issue view <number> --json ...
├── release-prep/run       # Show merged PRs since last tag
└── release-tag/run        # Tag a release

.claude/scripts/           # Symlinks (bin/ on the path)
├── goal-create → ../harness/goal-create/run
├── goal-list   → ../harness/goal-list/run
├── goal-get    → ../harness/goal-get/run
├── goal-queue  → ../harness/goal-queue/run
└── ...
```

### Design Principles

- **Harness owns all `gh` logic** — no actor ever calls `gh` directly
- **Structured output** — scripts return clean, parseable JSON
- **Scripts symlinks** — stable entry points so actors call by name
- **Directories** — each script can have sibling files (prompts, templates, config)
- **Minimal interface** — each script does one thing, takes simple arguments

### Story Scripts

| Script | Args | What it does |
|--------|------|-------------|
| `story-create` | `--title "..." < body.md` | Create an issue with `pipeline,story` labels |
| `story-list` | `[--state open\|closed\|all]` | List stories |
| `story-get` | `<number>` | Read a story and its linked goals |

### Goal Scripts

| Script | Args | What it does |
|--------|------|-------------|
| `goal-create` | `--story <number> --title "..." [--spot-check] < body.md` | Create an issue with `pipeline,goal,goal:draft` labels |
| `goal-list` | `[<status>]` | List goals, optionally filtered by status |
| `goal-get` | `<number>` | Read a goal's body and current status |
| `goal-queue` | `<number>` | Swap label: `goal:draft` → `goal:queued` |
| `goal-spot-check` | `<number> approve\|reject [--feedback "..."]` | Approve or reject a spot-check goal |

### Release Scripts

| Script | Args | What it does |
|--------|------|-------------|
| `release-prep` | | Show merged PRs since last tag, which stories are complete |
| `release-tag` | `<version>` | Tag a release |

### Actor Usage

The orchestrator loop:
```bash
goal=$(.claude/harness/goal-list queued | head -1)
.claude/harness/goal-queue "$goal"   # mark running
# clone, run ralph, cleanup
```

Claude during decomposition:
```bash
.claude/harness/goal-create --story 42 --spot-check --title "Add X" --body "..."
```

No `gh` knowledge needed in either actor.

## Ramp-Up Plan

### Phase 1 — Bottom-Up Build (current)

1. ~~Document the pipeline workflow (this document)~~
2. ~~Build goal scripts (goal-create, goal-list, goal-get, goal-queue, goal-spot-check)~~
3. ~~Build story scripts (story-create, story-list, story-get)~~
4. Add `--log-mode` to ralph (disable spinner, terminal codes, plain text output)
5. Build the orchestrator
6. Manual end-to-end test — hand-write a goal, queue it, orchestrator runs ralph, PR lands

### Phase 2 — Top-Down Dog-Fooding

7. Write the first story (scoped tight — single goal through the pipeline)
8. Decompose by hand (since `/decompose` doesn't exist yet)
9. Run the pipeline on real goals, observe and refine
10. Build the `/decompose` slash command
11. Build release scripts (release-prep, release-tag)
12. Scale up
