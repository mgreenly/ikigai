---
name: cdd
description: Context Driven Development pipeline for release workflows
---

# CDD Pipeline

This project calls its release workflow Context Driven Development (CDD)

## Workspace Configuration

**All CDD commands and scripts require the `$CDD_DIR` environment variable.**

```bash
export CDD_DIR=/path/to/workspace
```

This allows parallel workspaces without VCS conflicts. Each workspace is independent - different branches can use different workspace directories simultaneously.

Scripts will abort with a clear error if `$CDD_DIR` is not set.

## Core Philosophy

Use user/agent interactive development time during the Research and Plan phases to prepare tightly focused work that pre-loads the exact required context to run autonomously during the Execute phase.

## Pipeline Phases

  * Research
  * Plan
  * Execute

### 1. Research

**What:** Build foundational understanding before design decisions.

**Artifacts:**
- `$CDD_DIR/README.md` - High-level description of release features
- `$CDD_DIR/user-stories/` - User actions and expected system responses
- `$CDD_DIR/research/` - Facts gathered from internet, docs, codebase

**Why this phase exists:** Decisions made without research are assumptions. Research surfaces constraints, prior art, and edge cases that would otherwise appear during execution (when fixing is expensive).

**Key insight:** Research artifacts become reference material for execution. Sub-agents can read `$CDD_DIR/research/` without re-fetching from the internet. The research phase pays the lookup cost once.

### 2. Plan

**What:** Transform research into implementation decisions.

**Artifacts:**
- `$CDD_DIR/plan/` - Implementation decisions, architecture choices, detailed designs

**Why this phase exists:** Planning is the translation layer between "what we want" (research) and "what to do". It makes architectural decisions explicit.

### 3. Verify (Sub-phase of Plan)

**What:** Iterative verification of plan alignment.

**Artifacts:**
- `$CDD_DIR/verified.md` - Log of items verified and issues resolved

**Verification:**
```
/cdd:gap  # Verify README/user-stories → plan alignment
fix gaps
/cdd:gap  # Repeat until clean
```

Checks alignment down the pyramid:
- README/research/user-stories → plan
- Naming, error handling, memory management conventions
- Integration points, build changes, migrations

**Why this phase exists:** This is the **last moment of full context visibility**. Cross-cutting issues and inconsistencies can only be caught while the complete picture is visible.

**Key insights:**
- Specific checklists prevent vague "find issues" prompts
- `verified.md` prevents re-checking completed items

**Convergence pattern:** Each gap-finding run identifies the highest-priority issue. After fixing, re-run. When no substantive gaps remain, verification is complete.

### 4. Execute

**What:** Unattended execution via the Ralph harness script.

**Artifacts:**
- `$CDD_DIR/goals/*-goal.md` - Goal file(s) for ralph loop execution
- `$CDD_DIR/goals/*-progress.jsonl` - Auto-generated progress log
- `$CDD_DIR/goals/*-summary.md` - Auto-generated progress summary

**Ralph script:**
```bash
.claude/harness/ralph/run \
  --goal=$CDD_DIR/goals/implementation-goal.md \
  --duration=4h \
  --model=sonnet \
  --reasoning=low
```

**Process:**
Ralph iteratively works toward the goal until DONE or time expires:
1. Read goal + progress history
2. Make incremental progress (code changes, tests, fixes)
3. Commit progress via `jj commit`
4. Append progress to JSONL file
5. Repeat until objective achieved

**Why Ralph:** The plan phase prepared complete context. Ralph execution is mechanical - read plan, implement, test, commit. No research, no exploration, just steady progress toward the goal using the plan as a reference.

**Post-execution:**
After Ralph completes (or during iterations), quality checks and refactoring may be needed:
```
ralph → /check:quality → /refactor → /check:quality
```

## Directory Structure

```
$CDD_DIR/
├── README.md          # High-level release description (PRD)
├── user-stories/      # User actions and system responses
│   └── README.md      # Index/overview
├── research/          # Internet facts for reference
│   └── README.md      # Index/overview
├── plan/              # Implementation decisions
│   └── README.md      # Index/overview
├── verified.md        # Verification log (concerns + resolutions)
├── goals/             # Ralph goal files and execution state
│   ├── *-goal.md      # Goal definition(s)
│   ├── *-progress.jsonl   # Progress tracking (auto-generated)
│   └── *-summary.md   # Progress summary (auto-generated)
├── tmp/               # Temp files during execution
└── ...                # Other files permitted, ignored by pipeline
```

## Abstraction Levels

Each directory serves a distinct abstraction level. Content should not leak between levels.

### $CDD_DIR/plan/ - Coordination Layer

Plan documents **coordinate everything shared**.

**The plan owns:**
- **Public symbols** - Function names, struct names, enum names
- **Function signatures** - Arguments, argument order, return types
- **Struct members** - Field names, field order, field types
- **Enums** - Value names and meanings
- **Allowed libraries** - Which dependencies can be used, which are forbidden
- **Architectural choices** - Module boundaries, data flow, ownership rules

**The plan is the contract.**

**DO NOT include:** Implementation code, detailed algorithms, function bodies.

## Artifact Authority

Artifacts form a hierarchy of authority:

```
$CDD_DIR/README.md (authoritative)
        ↓
user-stories + research (derived)
        ↓
      plan (derived)
```

**Rules:**

1. Lower-level artifacts derive from higher-level ones
2. Contradictions between levels must be resolved immediately
3. Resolution is always top-down: align lower artifacts to higher authority
4. Higher-level documents may change through iteration—but when they do, all derived artifacts must be reviewed and realigned

**Why this matters:** Consistency is enforced during authoring because it cannot be enforced during execution.

## Writing Effective Ralph Goal Files

Ralph is NOT a single-shot agent with limited context. Ralph is an **iterative loop that can run indefinitely** until the goal is achieved. This fundamentally changes how you should write goal files.

### Ralph Has Unlimited Context

**Common mistake:** Treating Ralph like a limited-context agent
- ❌ Breaking down work into tiny steps because "the agent can't handle much"
- ❌ Avoiding references to multiple plan documents to "save context"
- ❌ Writing minimal goals thinking "I need to fit in X tokens"

**Correct approach:** Ralph iterates until done
- ✅ Reference ALL relevant plan documents - Ralph can read them across iterations
- ✅ Specify complete outcomes - Ralph will keep trying until achieved
- ✅ Include comprehensive acceptance criteria - Ralph uses them to verify completion
- ✅ List all relevant files - Ralph discovers and works through them iteratively

### What Ralph Needs in Goal Files

**Goal file format:**
```markdown
## Objective
Clear, complete description of what needs to be accomplished.
NOT a tiny incremental step - the FULL objective.

## Reference
List ALL relevant plan documents:
- $CDD_DIR/plan/architecture.md
- $CDD_DIR/plan/interfaces.md
- $CDD_DIR/plan/integration.md
- $CDD_DIR/research/api-spec.md
- $CDD_DIR/user-stories/feature-workflow.md

Ralph can read all of these. Don't artificially limit context.

## Outcomes
Specific, measurable outcomes that indicate completion:
- All functions in plan/interfaces.md implemented
- All tests in plan/testing.md passing
- Integration with existing code complete per plan/integration.md

## Acceptance
Clear success criteria:
- `make check` returns {"ok": true}
- All user stories in user-stories/ are satisfied
- No compiler warnings
```

### Goal File Principles

1. **Outcome-focused, not action-focused**
   - Good: "Implement web-fetch tool with HTML-to-markdown conversion per plan/web-fetch.md"
   - Bad: "Write the web_fetch_create function"

2. **Specify WHAT to achieve, not HOW to achieve it**
   - Never prescribe implementation order or steps
   - Never say "First do X, then do Y, finally do Z"
   - Instead: list measurable outcomes and let Ralph discover the path
   - Good: "All functions in plan/interfaces.md are implemented and tested"
   - Bad: "First write the structs, then the create function, then the tests"
   - Ralph will figure out the most effective order through iteration

3. **Reference liberally**
   - Reference every relevant plan document
   - Reference user stories that inform the work
   - Reference research that provides specifications
   - Ralph will read them as needed across iterations

3. **Complete acceptance criteria**
   - List all quality gates: `make check`, `make lint`, specific tests
   - Reference the verification that completion means
   - Be specific: not "tests pass" but "tests in tests/unit/web/ pass"

4. **Trust Ralph to iterate**
   - Don't micromanage the approach
   - Let Ralph discover the implementation path
   - Provide the destination (outcomes), not the route

5. **One goal = one cohesive objective**
   - Not artificially small steps
   - But also not "implement the entire release"
   - A goal should be: "Implement feature X per plan/X.md"

### Example: Good vs Bad Goals

**Bad Goal (too limited, assumes constrained context):**
```markdown
## Objective
Create the web_fetch_t struct.

## Reference
See plan/web-fetch.md section 2.1

## Outcomes
- Struct defined in src/web_fetch.h
```

**Good Goal (comprehensive, leverages unlimited context):**
```markdown
## Objective
Implement the web-fetch tool with HTML-to-markdown conversion, supporting
both URL fetching and content processing as specified in plan/web-fetch.md.

## Reference
- $CDD_DIR/plan/web-fetch.md - Complete interface and behavior spec
- $CDD_DIR/plan/tool-integration.md - Integration with tool registry
- $CDD_DIR/plan/html-markdown.md - Conversion library choices and approach
- $CDD_DIR/research/html-to-markdown.md - Library comparison and decision rationale
- $CDD_DIR/user-stories/web-fetch.md - Expected user interaction

## Outcomes
- web_fetch tool implemented per plan/web-fetch.md interfaces
- HTML-to-markdown conversion working per plan/html-markdown.md
- Integrated with tool registry per plan/tool-integration.md
- All unit tests in tests/unit/web_fetch/ passing
- User stories in user-stories/web-fetch.md satisfied

## Acceptance
- `make check` passes
- `make lint` passes
- Manual test: `./ikigai` → `/web-fetch https://example.com` returns markdown
```

### Why This Matters

The plan phase spent tokens researching and designing. The goal file connects Ralph to that investment. If you write a minimal goal, Ralph will either:
- Fail because it lacks necessary context
- Re-research what you already documented (wasting tokens)
- Make decisions inconsistent with the plan

**Write goals that leverage the plan's completeness. Ralph will iterate until the comprehensive objective is achieved.**

## Lifecycle

```
Create empty $CDD_DIR/ → Research → Plan → Verify → Execute → Delete $CDD_DIR/
```

The workspace directory is ephemeral. It exists only for the duration of the release workflow. After successful execution, it's deleted. The work lives in the codebase; the pipeline artifacts are disposable.

## Efficiency Principles

1. **Load skills on-demand** - Don't preload "just in case"
2. **Complete authoring** - Spend tokens researching during authoring so execution doesn't need to
3. **Reference vs working knowledge** - Large docs go in separate skills, loaded when needed

## Design Tradeoffs

| Decision | Tradeoff | Rationale |
|----------|----------|-----------|
| Full context in verify | Expensive per-run | Last chance to catch cross-cutting issues |
| Verification is human-terminated | Subjective | Models don't converge to "done" naturally |
| Research cached in files | Stale if release spans days | Avoids re-fetching during execution |

## When to Load This Skill

- Designing or modifying the release workflow
- Debugging why execution sub-agents are failing
- Optimizing token usage in the pipeline
- Understanding why a phase exists before changing it
