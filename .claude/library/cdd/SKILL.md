---
name: cdd
description: Context Driven Development pipeline for release planning
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

Use user/agent interactive development time during the Research and Plan phases to build complete, verified specifications before implementation begins.

## Pipeline Phases

  * Research
  * Plan (with Verify sub-phase)

### 1. Research

**What:** Build foundational understanding before design decisions.

**Artifacts:**
- `$CDD_DIR/README.md` - High-level description of release features
- `$CDD_DIR/user-stories/` - User actions and expected system responses
- `$CDD_DIR/research/` - Facts gathered from internet, docs, codebase

**Why this phase exists:** Decisions made without research are assumptions. Research surfaces constraints, prior art, and edge cases that would otherwise appear during implementation (when fixing is expensive).

**Key insight:** Research artifacts become reference material. The research phase pays the lookup cost once.

### 2. Plan

**What:** Transform research into implementation decisions.

**Artifacts:**
- `$CDD_DIR/plan/` - Implementation decisions, architecture choices, detailed designs

**Why this phase exists:** Planning is the translation layer between "what we want" (research) and "what to build". It makes architectural decisions explicit.

### 3. Verify (Sub-phase of Plan)

**What:** Iterative verification following the artifact pyramid.

**Artifacts:**
- `$CDD_DIR/verified.md` - Log of items verified and issues resolved

**Process:**
```
/cdd:gap-plan  # Verify README/user-stories → plan alignment
fix gaps
/cdd:gap-plan  # Repeat until clean
```

Checks alignment down the pyramid:
- README/research/user-stories → plan
- Naming, error handling, memory management conventions
- Integration points, build changes, migrations

**Why this phase exists:** Verification catches inconsistencies while the complete picture is visible.

**Key insights:**
- Specific checklists prevent vague "find issues" prompts
- `verified.md` prevents re-checking completed items
- Token budgets (140k per command) enable thorough verification

**Convergence pattern:** Each gap-finding run identifies the highest-priority issue. After fixing, re-run. When no substantive gaps remain, verification is complete.

## Directory Structure

```
$CDD_DIR/
├── README.md          # High-level release description
├── user-stories/      # User actions and system responses
│   └── README.md      # Index/overview
├── research/          # Internet facts for reference
│   └── README.md      # Index/overview
├── plan/              # Implementation decisions
│   └── README.md      # Index/overview
├── verified.md        # Verification log (concerns + resolutions)
└── ...                # Other files permitted
```

## Abstraction Levels

Each directory serves a distinct abstraction level. Content should not leak between levels.

### $CDD_DIR/plan/ - Coordination Layer

Plan documents **coordinate shared contracts and decisions**.

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

## Lifecycle

```
Create empty $CDD_DIR/ → Research → Plan → Verify → [Implementation outside CDD] → Delete $CDD_DIR/
```

The workspace directory is ephemeral. It exists only for the duration of the planning workflow. After the plan is complete and verified, the work transitions to implementation (outside CDD scope). The CDD artifacts are disposable.

## Efficiency Principles

1. **Load skills on-demand** - Don't preload "just in case"
2. **Reference vs working knowledge** - Large docs go in separate skills, loaded when needed

## When to Load This Skill

- Designing or modifying the release workflow
- Understanding why a phase exists before changing it
- Working with CDD workspace artifacts
