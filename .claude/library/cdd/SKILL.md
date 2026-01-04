---
name: cdd
description: Context Driven Development pipeline for release workflows
---

# CDD Pipeline

This project calls its release workflow Context Driven Development (CDD)

## Core Philosophy

Use user/agent interactive development time during the Research and Plan phases to prepare tightly focused tasks that pre-load the exact required context to run autonomously during the Implement phase.

## Pipeline Phases

  * Research
  * Plan
  * Execute

### 1. Research

**What:** Build foundational understanding before design decisions.

**Artifacts:**
- `cdd/README.md` - High-level description of release features
- `cdd/user-stories/` - User actions and expected system responses
- `cdd/research/` - Facts gathered from internet, docs, codebase

**Why this phase exists:** Decisions made without research are assumptions. Research surfaces constraints, prior art, and edge cases that would otherwise appear during execution (when fixing is expensive).

**Key insight:** Research artifacts become reference material for task execution. Sub-agents can read `cdd/research/` without re-fetching from the internet. The research phase pays the lookup cost once.

### 2. Plan

**What:** Transform research into implementation decisions and executable task files.

**Artifacts:**
- `cdd/plan/` - Implementation decisions, architecture choices, detailed designs
- `cdd/tasks/` - Individual task files (one per unit of work)
- `cdd/tasks/order.json` - Execution order and task metadata

**Why this phase exists:** Planning is the translation layer between "what we want" (research) and "what to do" (tasks). It makes architectural decisions explicit and breaks work into parallelizable units.

**Key insight:** Task files must be complete. During execution, sub-agents see only their task file. Any context not written into the task must be re-discovered (wasting tokens) or will be missed (creating bugs).

### 3. Verify (Sub-phase of Plan)

**What:** Iterative review of the complete plan before committing to execution.

**Artifacts:**
- `cdd/verified.md` - Log of concerns raised and resolved

**Process:**
```
while (concerns feel substantive):
    ask: "What is the most likely reason this plan will not succeed?"
    if concern is substantive:
        fix interactively with agent
        log concern as resolved in verified.md
    else:
        done (concerns have become artificial)
```

**Why this phase exists:** This is the **last moment of full context visibility**. Once execution begins, each sub-agent sees only its task. Cross-cutting issues, inconsistencies between tasks, missing dependencies - these can only be caught while the complete picture is visible.

**Key insights:**
- The question is intentionally open-ended, leveraging LLM reasoning over checklists
- `verified.md` prevents re-raising resolved concerns in subsequent iterations
- Termination is by human judgment: when suggestions shift from "this will cause problems" to "well theoretically..." - that's the signal
- Models don't naturally say "we're done" - they manufacture concerns indefinitely if pushed

**Convergence pattern:** Each fix surfaces the next-highest-priority concern. Quality improves until objections become artificial. This is convergence by diminishing quality of objections, not absence of objections.

### 4. Execute

**What:** Mechanical execution of task files via orchestration loop.

**Process:**
```
/orchestrate → /check-quality → /refactor → /check-quality
```

**Why this structure:** Execution is unattended. No human to provide missing context. Task files must be complete or sub-agents will fail, research (wasting tokens), or succeed partially (creating debt).

**Key insight:** Orchestrators load ZERO skills. They're pure execution loops. Sub-agents read task files directly - task content never flows through orchestrator context.

## Directory Structure

```
cdd/
├── README.md          # High-level release description
├── user-stories/      # User actions and system responses
│   └── README.md      # Index/overview
├── research/          # Internet facts for task reference
│   └── README.md      # Index/overview
├── plan/              # Implementation decisions
│   └── README.md      # Index/overview
├── tasks/             # Individual task files
│   └── order.json     # Execution order
├── verified.md        # Verification log (concerns + resolutions)
├── tmp/               # Temp files during execution
└── ...                # Other files permitted, ignored by pipeline
```

## Abstraction Levels

Each directory serves a distinct abstraction level. Content should not leak between levels.

### cdd/plan/ - Coordination Layer

Plan documents **coordinate everything shared between tasks**. If two tasks must agree on something, the plan decides it.

**The plan owns:**
- **Public symbols** - Function names, struct names, enum names
- **Function signatures** - Arguments, argument order, return types
- **Struct members** - Field names, field order, field types
- **Enums** - Value names and meanings
- **Allowed libraries** - Which dependencies can be used, which are forbidden
- **Architectural choices** - Module boundaries, data flow, ownership rules

**Why this matters:** Tasks execute independently. If the plan doesn't coordinate shared symbols, Task A might define `foo_create(ctx, name, count)` while Task B expects `foo_create(ctx, count, name)`. Both compile. Integration fails.

**The plan is the contract.** Tasks implement what the plan specifies. Disagreements are resolved in the plan before tasks are written.

**DO NOT include:** Implementation code, detailed algorithms, function bodies.

### cdd/tasks/ - Implementation Units

Task files specify **what this task implements** from the plan:
- Which functions from the plan this task implements
- Which structs from the plan this task defines
- Expected behaviors and edge cases
- Test scenarios for this task's scope

**Tasks do NOT make coordination decisions.** If a task needs a function signature, struct layout, or enum value - it comes from the plan. Tasks copy the relevant specs from the plan and add implementation guidance.

**DO NOT include:** Implementation code, function bodies, working test code.

The sub-agent writes the actual code. The task tells them what to write and how to test it.

**Why this separation matters:** Tasks execute independently without seeing each other. The plan is their shared reference. If Task A invents a struct layout instead of copying from the plan, Task B (which also uses that struct) will have a different layout. The plan prevents this.

## Artifact Authority

Artifacts form a hierarchy of authority:

```
cdd/README.md (authoritative)
        ↓
user-stories + research (derived)
        ↓
      plan (derived)
        ↓
      tasks (derived)
```

**Rules:**

1. Lower-level artifacts derive from higher-level ones
2. Contradictions between levels must be resolved immediately
3. Resolution is always top-down: align lower artifacts to higher authority
4. Higher-level documents may change through iteration—but when they do, all derived artifacts must be reviewed and realigned

**Why this matters:** Sub-agents executing tasks cannot detect contradictions with higher-level intent. If a task contradicts the plan, or the plan contradicts the README, the error propagates silently. Consistency is enforced during authoring because it cannot be enforced during execution.

## Lifecycle

```
Create empty cdd/ → Research → Plan → Verify → Execute → Delete cdd/
```

The `cdd/` directory is ephemeral. It exists only for the duration of the release workflow. After successful execution, it's deleted. The work lives in the codebase; the pipeline artifacts are disposable.

## Efficiency Principles

1. **Orchestrators load ZERO skills** - Pure execution loops need only embedded logic
2. **Sub-agents read task files** - Never pass task content through orchestrator context
3. **Load skills on-demand** - Don't preload "just in case"
4. **Complete task authoring** - Spend tokens researching during authoring so execution doesn't need to
5. **Reference vs working knowledge** - Large docs go in separate skills, loaded when needed

## Design Tradeoffs

| Decision | Tradeoff | Rationale |
|----------|----------|-----------|
| Full context in verify | Expensive per-run | Last chance to catch cross-cutting issues |
| Task files are complete | Verbose, some duplication | Sub-agents can't see other tasks |
| Verification is human-terminated | Subjective | Models don't converge to "done" naturally |
| Research cached in files | Stale if release spans days | Avoids re-fetching during execution |

## When to Load This Skill

- Designing or modifying the release workflow
- Debugging why execution sub-agents are failing
- Optimizing token usage in the pipeline
- Understanding why a phase exists before changing it
