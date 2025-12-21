Autonomous refactoring pipeline that finds and executes ONE high-impact, behavior-preserving code improvement.

**Usage:** `/refactor`

Analyzes `src/`, identifies the SINGLE most impactful refactoring target, generates tasks in scratch/tasks/, and executes. Run again for more - each run handles one target.

## CRITICAL CONSTRAINTS

1. **Behavior-preserving only** - No test changes. `make check` must pass.
2. **Sequential execution** - One task at a time.
3. **Uses scratch/** - Plan in scratch/plan/, tasks in scratch/tasks/, logs in scratch/details.log.

---

You are the refactoring orchestrator.

## PHASE 0: PRECHECKS

Run in order. If ANY fails, report and **STOP**:

1. `git status --porcelain` - abort if uncommitted changes
2. Check scratch/tasks/order.json - abort if pending tasks exist
3. `make lint` - abort if fails
4. `make check` - abort if fails

## PHASE 1: SETUP

```bash
mkdir -p scratch/plan scratch/tasks scratch/tmp
echo "[$(date -Iseconds)] REFACTOR START: commit=$(git rev-parse HEAD)" > scratch/details.log
.claude/library/task/init.ts
```

## PHASE 2: ANALYZE

Spawn ONE sub-agent (opus, NOT in background):

```
You are a refactoring analyst for a C codebase.

Load skills: /load refactoring/smells, /load refactoring/techniques, /load naming, /load style

Analyze src/ to find the MOST IMPACTFUL refactoring opportunity.

CONSTRAINTS:
- Behavior-preserving ONLY - no test changes
- No public API changes
- Focus on: naming, style, memory patterns, code smells

Select ONE target only. Write scratch/plan/README.md with:
- **Target**: One-line description
- **What**: Exactly what changes (files, patterns)
- **How**: Transformation approach
- **Why**: Why highest-impact
- **Files**: List of files to modify

Log decisions to scratch/details.log.
Return {"ok": true} when complete.
```

## PHASE 3: GENERATE TASKS

Spawn ONE sub-agent (opus):

```
Load skills: /load task-authoring, /load refactoring/techniques

Read scratch/plan/README.md. Generate ONE TASK PER FILE in scratch/tasks/.
Create scratch/tasks/order.json with model=sonnet, thinking=none.

Return {"ok": true} when complete.
```

## PHASE 4: REVIEW

Spawn ONE sub-agent (sonnet):

```
Review scratch/tasks/*.md for:
- Self-contained with all context
- Correct order in order.json
- Appropriate model/thinking levels

Fix any issues. Return {"ok": true}.
```

## PHASE 5: EXECUTE

Import and run:

```bash
.claude/library/task/import.ts
```

Then follow the same execution loop as /orchestrate (get next, start, spawn agent, done/escalate, repeat).

## COMPLETION

Log summary to scratch/details.log. Show stats. Report:

```
Refactoring complete.
- Completed: X tasks
- Failed: Y tasks
- Log: scratch/details.log

Validate with: make check && make lint
```

Begin refactoring now.
