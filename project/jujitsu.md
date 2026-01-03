# Jujutsu (jj) Stacked-Diff Workflow

Exploration of replacing git with jj to enable a stacked-diff workflow with parallel agent execution.

## Motivation

Current workflow is sequential: plan → execute task 1 → execute task 2 → ... Each task runs the full quality harness before the next begins. This is safe but slow.

Stacked diffs allow:
- Forward agent coding ahead while quality agents test behind
- Parallel execution across the stack
- Fixes propagating forward automatically via rebase

## Proposed Agent Model

```
coder:     bookmark-1 → bookmark-2 → bookmark-3 → bookmark-4 → ...
                ↓
unit:      test → fix → pass
                     ↓
sanitize:  test → fix → pass
                     ↓
coverage:  test → fix → pass
                     ↓
memcheck:  test → fix → merge to main
```

### Agent Responsibilities

| Agent | Tests | Fixes | Merges | Updates Plan |
|-------|-------|-------|--------|--------------|
| coder | `make check` only | own work | no | no |
| unit | full unit tests | yes | no | yes |
| sanitize | ASan + UBSan | yes | no | yes |
| coverage | line + branch | yes | no | yes |
| memcheck | valgrind + helgrind | yes | yes | yes |

### Conflict Resolution

When an agent fixes code, jj automatically rebases downstream bookmarks. If conflicts arise:
- The fixing agent resolves conflicts in its immediate downstream bookmark
- Conflicts further down remain marked; next agent in pipeline resolves when it reaches that bookmark

### Plan Evolution

The plan is no longer frozen after verify phase. Each agent that makes a fix records what changed and why:

```
release/plan/
├── README.md           # Current authoritative plan
├── CHANGELOG.md        # Evolution log
│   └── [bookmark-1] unit: changed X because learned Y
│   └── [bookmark-1] sanitize: changed Z because sanitizer found W
└── *.md                # Individual plan documents
```

Tasks reference plan version. Later tasks in the stack read updated plan.

## Migration and Reversibility

jj can run alongside git (colocated) or replace it entirely (non-colocated).

### Colocated Mode (Recommended for Experimenting)

```bash
# In existing git repo
jj git init --colocate

# Now you have both:
# .git/  (still works, still valid)
# .jj/   (jj's data)
```

While colocated:
- `jj` commands work
- `git` commands still work
- Push/pull to GitHub works from either
- jj automatically syncs changes to .git

To revert to pure git:
```bash
rm -rf .jj
# Done. Back to pure git. Nothing lost.
```

### Non-Colocated Mode

```bash
# Fresh clone via jj (no .git directory)
jj git clone git@github.com:mgreenly/ikigai.git
```

Only `.jj` exists. Git commands don't work locally, but GitHub still works:
```bash
jj git fetch
jj git push
```

### Comparison

| Mode | Local Commands | GitHub | Revert to Git |
|------|----------------|--------|---------------|
| Colocated | git + jj both work | works | `rm -rf .jj` |
| Non-colocated | jj only | works | re-clone with git |

**Recommendation:** Start colocated. Low risk. If jj proves valuable long-term, consider non-colocated for cleaner setup (no sync overhead).

## Migration from Bare Repo + Worktrees

Current git structure uses a bare repo as parent with worktrees nested inside:

```
ikigai.git/           # bare repo (parent)
├── rel-07/           # git worktree
├── rel-08/           # git worktree
└── main/             # git worktree
```

jj uses sibling workspaces instead:

```
~/projects/
├── ikigai/           # main workspace (holds .jj/)
├── ikigai-rel-08/    # jj workspace
└── ikigai-rel-11/    # jj workspace
```

### Migration Steps

1. **Pick a location for the main workspace:**
   ```bash
   cd ~/projects
   git clone git@github.com:mgreenly/ikigai.git ikigai
   cd ikigai
   ```

2. **Initialize jj in colocated mode:**
   ```bash
   jj git init --colocate
   ```

3. **Create workspaces for parallel features:**
   ```bash
   jj workspace add ../ikigai-rel-08 --name rel-08
   jj workspace add ../ikigai-rel-11 --name rel-11
   ```

4. **Set up each workspace on its bookmark:**
   ```bash
   cd ../ikigai-rel-08
   jj new main -m "rel-08-tools-01: first bookmark"
   jj bookmark set rel-08-tools-01

   cd ../ikigai-rel-11
   jj new main -m "rel-11-assets-01: first bookmark"
   jj bookmark set rel-11-assets-01
   ```

### What Changes

| Aspect | Git Bare + Worktrees | jj Workspaces |
|--------|---------------------|---------------|
| Repo data location | `ikigai.git/` (bare) | `ikigai/.jj/` (in main workspace) |
| Worktree location | Nested in bare repo | Siblings of main workspace |
| Creating worktrees | `git worktree add` | `jj workspace add` |
| Switching context | `cd` to different worktree | `cd` to different workspace |

### Keeping Old Structure During Transition

You can keep the old bare repo structure alongside while experimenting:

```
~/projects/
├── ikigai.git/           # old bare repo (keep for safety)
│   ├── rel-07/           # existing worktree
│   └── main/
├── ikigai/               # new jj main workspace
├── ikigai-rel-08/        # jj workspace
└── ikigai-rel-11/        # jj workspace
```

Once comfortable with jj, remove the old structure.

## jj Primitives We Rely On

### Stable Change IDs

Every change has a stable ID that survives rebases. Agents reference changes by ID, not commit SHA.

```bash
jj log --no-graph -T 'change_id ++ " " ++ description.first_line() ++ "\n"'
# kkmpptxz implement foo parser
# rrslvtxy add foo tests
# etc.
```

### Automatic Rebase

When a change is modified, all descendants rebase automatically in jj's view:

```bash
jj rebase -s <change-id> -d main   # rebase change and all descendants onto main
```

### Conflicts as First-Class State

jj allows commits with unresolved conflicts. The conflict is data, not a blocking error:

```bash
jj status   # shows conflicted files
jj resolve  # interactive resolution
```

This means: sanitize's fix might conflict with bookmark-4, but bookmark-4 still exists (marked conflicted). The agent working on bookmark-4 sees the conflict and resolves it.

### No Staging Area

Every working state is automatically a commit. No "uncommitted work" to lose:

```bash
jj status          # shows current change
jj describe -m "message"  # update commit message
jj new             # start new change on top
```

### Editing Changes (not "checkout")

jj doesn't have git-style checkout. You "edit" a change - making it the one your working copy reflects:

```bash
jj edit kkmpptxz    # edit by change ID
jj edit bookmark-1  # edit by bookmark name
jj new bookmark-1   # create NEW change on top of bookmark-1 and edit that
```

Mental model: there's no "current branch." There's "the change I'm currently editing."

### Workspaces (for parallel agents)

jj workspaces are like git worktrees - separate working directories sharing the same repo:

```bash
# From main repo, create workspace for unit testing
jj workspace add ../ikigai-unit --name unit

# Unit test agent works in separate directory
cd ../ikigai-unit
jj edit bookmark-1   # edit this change in this workspace
```

Each workspace:
- Has its own working copy
- Shares the same repo data (changes, bookmarks, history)
- Can edit a different change simultaneously
- Sees changes from other workspaces immediately (shared operation log)

**For multi-agent setup:**

```
~/projects/ikigai/            # coder (main workspace)
~/projects/ikigai-unit/       # unit tests
~/projects/ikigai-sanitize/   # ASan + UBSan
~/projects/ikigai-coverage/   # line + branch coverage
~/projects/ikigai-memcheck/   # valgrind + helgrind
```

Workspace names match harness directory names. Self-documenting.

Each agent runs in its own terminal, own workspace, editing different bookmarks. No file conflicts.

## Bookmark Naming Convention

Format: `<release>-<feature>-<sequence>`

Examples:
```
rel08-auth-01
rel08-auth-02
rel08-auth-03
```

### Rules

1. **Release prefix** - Ties bookmark to release (e.g., `rel08`). Prevents collision across concurrent work.
2. **Feature slug** - Short identifier for the feature (e.g., `auth`, `parser`, `ui-refresh`).
3. **Zero-padded sequence** - Two digits (`01`, `02`, ..., `99`). Ensures correct sort order.

### Why not descriptive suffixes?

```
rel08-auth-01-session-store    # longer, but readable
rel08-auth-01                  # shorter, task file has description
```

Keep bookmarks short. The task file (`release/tasks/rel08-auth-01.md`) contains the full description. Bookmarks are references, not documentation.

### Sequence indicates stack order

The sequence number reflects position in the stack:
- `rel08-auth-01` is the base
- `rel08-auth-02` builds on `01`
- `rel08-auth-03` builds on `02`

When memcheck merges `01`, bookmarks `02` and `03` rebase onto main automatically.

## Workflow Simulation: Happy Path

Setup: Feature requires 4 sub-changes. Coder writes code, quality agents test in pipeline.

### T0: Coder starts

```bash
jj new main -m "bookmark-1: add foo parser"
# ... writes code ...
make check  # passes
jj bookmark set bookmark-1
jj new -m "bookmark-2: add foo serializer"
```

### T1: Coder continues, unit agent starts on bookmark-1

**Coder:**
```bash
# ... writes bookmark-2 code ...
make check  # passes
jj bookmark set bookmark-2
jj new -m "bookmark-3: add foo integration"
```

**Unit agent** (in `ikigai-unit/` workspace):
```bash
jj edit bookmark-1
make check  # full test suite - passes
# no fixes needed, pass to sanitize
```

### T2: Agents continue down the pipeline

**Coder:** working on bookmark-3

**Unit agent:** starts on bookmark-2

**Sanitize agent** (in `ikigai-sanitize/` workspace):
```bash
jj edit bookmark-1
make check-sanitize  # passes
# pass to coverage
```

### T3: Memcheck agent merges bookmark-1

**Memcheck agent** (in `ikigai-memcheck/` workspace):
```bash
jj edit bookmark-1
make check-valgrind && make check-helgrind  # passes
jj rebase -r bookmark-1 -d main
jj bookmark set main -r bookmark-1
jj git push
```

Bookmark-1 is now merged. Bookmarks 2, 3, 4 automatically rebase onto new main.

## Workflow Simulation: Fix Required

### T1: Unit agent finds failure in bookmark-1

```bash
jj edit bookmark-1
make check  # FAILS: foo_parse() doesn't handle empty input
```

Unit agent fixes:
```bash
# ... edit foo_parse.c ...
jj describe -m "bookmark-1: add foo parser (fix empty input)"
```

### T2: Fix propagates

jj automatically rebases bookmark-2, bookmark-3 onto the amended bookmark-1.

Coder, working on bookmark-4, periodically checks:
```bash
jj status  # shows if working copy needs update
jj rebase -d bookmark-3  # if bookmark-3 moved, rebase current work
```

### T3: What if fix conflicts with bookmark-2?

Unit agent's fix changed a function signature. Bookmark-2 uses old signature.

```bash
jj log  # shows bookmark-2 marked as conflicted
```

Options:
1. **Unit agent resolves immediately** - it has context on the fix
2. **Let unit agent (testing bookmark-2) resolve** - it will see the conflict when it edits that change

Proposal: Agent doing the fix resolves conflicts in immediate children only. Deeper conflicts wait for their turn in the pipeline.

## Workflow Simulation: Coder Checks for Updates

Coder should check before starting each new bookmark:

```bash
# After completing bookmark-N, before starting bookmark-N+1:
jj fetch
jj log -r 'main..@'  # see if main advanced
# If main advanced (prior bookmarks merged):
jj rebase -d main
```

This pulls in any merged changes and keeps the coder's stack based on current main.

## Open Questions

### Q1: How often should coder check for updates?

Options:
- Before each new bookmark (proposed)
- On a timer
- Only when explicitly requested

### Q2: Should fixes go back through the pipeline?

If sanitize agent fixes bookmark-1, should it go back through unit agent?

Options:
- **Forward only**: Trust the fix, continue to coverage. Risk: fix might break unit tests.
- **Back to unit**: Creates loops, but guarantees quality at each layer.
- **Unit runs in parallel**: Sanitize fix triggers new unit run, but sanitize continues to coverage. Merge blocked until both unit and coverage pass.

### Q3: How do we track which bookmark is at which pipeline stage?

Need a status file or similar:

```
release/pipeline-status.json
{
  "bookmark-1": {"stage": "memcheck", "status": "testing"},
  "bookmark-2": {"stage": "sanitize", "status": "fixing"},
  "bookmark-3": {"stage": "unit", "status": "queued"},
  "bookmark-4": {"stage": "coder", "status": "coding"}
}
```

### Q4: What happens if coder runs out of planned work?

If coder finishes all planned bookmarks but bookmark-1 is still in the pipeline:
- Wait for pipeline to complete
- Start on next feature (risky - might conflict)
- Help with fixes in the pipeline

### Q5: How do we handle pipeline failures that can't be fixed?

If memcheck agent can't fix a valgrind issue after escalation:
- Block the pipeline
- Notify human
- Coder should probably stop too (building on broken foundation)

## Changes Required to .claude/

### New: jj skill

Replace git skill with jj equivalents:
- `jj new`, `jj describe`, `jj bookmark`
- `jj rebase` for stack management
- `jj git push` for remote sync

### Modified: cdd skill

- Plan versioning with CHANGELOG.md
- Bookmark-aware execution model
- Pipeline stage tracking

### Modified: task skill

- Replace file-based status with jj bookmark status
- Track pipeline position per bookmark
- Handle conflicts as a state

### New: pipeline skill

Orchestration for multi-agent pipeline:
- Spawn unit, sanitize, coverage, memcheck agents
- Track bookmark → stage mapping
- Handle fix propagation
- Coordinate merges

### Modified: harness

Each harness check becomes a pipeline stage instead of a sequential step.

---

## Alternative: Parallel Quality Checks (No jj Required)

The stacked-diff model has a fundamental problem: fixes at later pipeline stages can break earlier stages, creating circular dependencies. A simpler parallelism model avoids this entirely.

### The Idea

Instead of parallelizing across bookmarks, parallelize across quality checks within a single bookmark:

```
bookmark created
     ↓
┌────┴────┬──────────┬──────────┬──────────┬──────────┐
↓         ↓          ↓          ↓          ↓          ↓
lint    unit    sanitize   coverage   valgrind   helgrind
↓         ↓          ↓          ↓          ↓          ↓
└────┬────┴──────────┴──────────┴──────────┴──────────┘
     ↓
all pass? → merge
any fail? → fix → re-run ALL in parallel again
```

### Benefits

1. **Best case**: All pass. Wall-clock time = slowest check (not sum of all checks).
2. **Find failures faster**: If helgrind would fail, you find out immediately instead of waiting for lint → unit → sanitize → coverage → valgrind to complete first.
3. **No circular dependency**: Any failure triggers fix + full re-run. Simple loop.
4. **No jj required**: Works with git. No migration needed.
5. **Simple mental model**: Run everything, wait, fix, repeat.

### Current Sequential Model

```
lint (2s) → unit (30s) → sanitize (45s) → coverage (60s) → valgrind (90s) → helgrind (90s)
Total: ~5 minutes sequential
```

If helgrind fails, you wait 5 minutes to find out.

### Parallel Model

```
lint ────────────────────────────────────────────────────→ (2s)
unit ────────────────────────────────────────────────────→ (30s)
sanitize ────────────────────────────────────────────────→ (45s)
coverage ────────────────────────────────────────────────→ (60s)
valgrind ────────────────────────────────────────────────→ (90s)
helgrind ────────────────────────────────────────────────→ (90s)
                                                           ─────
Total: ~90 seconds (bounded by slowest)
```

If helgrind fails, you find out in 90 seconds, not 5 minutes.

### Requirements

#### 1. Database Isolation

Each build variant needs its own database to avoid test conflicts:

```
ikigai_test_unit
ikigai_test_sanitize
ikigai_test_coverage
ikigai_test_valgrind
ikigai_test_helgrind
```

Connection string includes build variant. Tests don't collide.

#### 2. Build Directory Isolation

Each variant may need separate build artifacts:

```
build/
├── unit/
├── sanitize/
├── coverage/
├── valgrind/
└── helgrind/
```

Or use `make clean` between variants (slower, but simpler).

#### 3. Resource Capacity

Running all checks in parallel requires:
- Enough CPU cores (6 parallel builds)
- Enough memory (6 concurrent test suites)
- Enough database connections (6 isolated DBs)

### Implementation Sketch

```bash
# parallel-harness.sh
run_check() {
    local variant=$1
    export IKIGAI_TEST_DB="ikigai_test_${variant}"
    export IKIGAI_BUILD_DIR="build/${variant}"
    make check-${variant} > "logs/${variant}.log" 2>&1
    echo $? > "results/${variant}.status"
}

# Run all in parallel
run_check unit &
run_check sanitize &
run_check coverage &
run_check valgrind &
run_check helgrind &

wait  # Wait for all to complete

# Check results
for variant in unit sanitize coverage valgrind helgrind; do
    if [ "$(cat results/${variant}.status)" != "0" ]; then
        echo "FAILED: ${variant}"
        cat "logs/${variant}.log"
        exit 1
    fi
done

echo "All checks passed"
```

### Tradeoffs vs Stacked-Diff Model

| Aspect | Stacked-Diff (jj) | Parallel Checks |
|--------|-------------------|-----------------|
| Parallelism | Across bookmarks | Across checks |
| Complexity | High (coordination) | Low (just parallel jobs) |
| Fix propagation | Complex (circular deps) | Simple (re-run all) |
| Migration required | Yes (git → jj) | No |
| Resource usage | Moderate | High (6x parallel) |
| Time to first failure | Depends on stage | Always minimal |

### Conclusion

This approach is simpler and achieves the main goal: faster feedback. The stacked-diff model optimizes for coder throughput, but the circular dependency problem makes it impractical for heavy quality gates.

Parallel checks optimize for quality feedback speed, which may be more valuable: find problems faster, fix them faster, iterate faster.

## Next Steps

1. ~~Install jj and experiment with basic stacked workflow~~ (on hold)
2. ~~Simulate conflict scenarios manually~~ (on hold)
3. ~~Design pipeline status tracking~~ (on hold)
4. ~~Prototype single-agent jj workflow before multi-agent~~ (on hold)

**For parallel checks approach:**
1. Audit database connection handling in tests
2. Design database naming scheme per build variant
3. Test build directory isolation (or measure `make clean` overhead)
4. Prototype parallel harness script
5. Measure resource requirements (CPU, memory, disk)
