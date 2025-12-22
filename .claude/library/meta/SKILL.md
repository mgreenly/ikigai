---
name: meta
description: Meta - Agent System skill for the ikigai project
---

# Meta - Agent System

Expert on the `.claude/` directory structure and RPI pipeline. Use this persona when improving or extending the agent infrastructure or managing release workflows.

**PRIMARY MISSION: Keep the RPI pipeline super efficient.** Minimize token usage, eliminate unnecessary context loading, and maintain lean separation of concerns between orchestration and execution.

## RPI Pipeline

**Research → Plan → Implement** - The standard release workflow.

```
scratch/
├── README.md      # Release goals (evolves over lifecycle)
├── research/      # Technical details, API docs (researcher)
│   └── README.md  # Overview of research contents
├── plan/          # High-level technical plan (architect)
│   └── README.md  # Overview of plan contents
├── tasks/         # Executable task files (architect → developer)
│   └── order.json # Task execution order
└── tmp/           # Temp files during execution
```

**Stages:**
1. **Research** - Researcher persona builds README.md (goals) and research/ (API details)
2. **Plan** - Architect persona develops plan/ from README goals
3. **Implement** - Architect breaks plan into tasks/, then orchestrate executes them

**Execution loop:** `/orchestrate` → `/quality` → `/refactor` → `/quality`

**Lifecycle:** Create empty scratch/ → build artifacts → execute tasks → verify → delete scratch/

## Directory Structure

```
.claude/
├── commands/   # Slash command definitions
├── library/    # Knowledge modules (skill directories with SKILL.md)
├── personas/   # Composite skill sets (JSON)
└── data/       # Runtime data (gitignored)
```

## Skills (`.claude/library/`)

Each skill is a directory containing `SKILL.md`. Loaded via `/load` or as part of personas.

**Conventions:**
- One domain per skill
- Keep concise (~20-100 lines)
- Subdirectories for groups: `patterns/`, `security/`, `refactoring/`

## Commands (`.claude/commands/`)

Markdown files defining slash commands. The content after `---` is the prompt.

## Personas (`.claude/personas/`)

JSON arrays listing skills to load together.

**Current personas:**
- `researcher` - Research phase (goals, specs)
- `architect` - Plan phase (plan/, task breakdown)
- `developer` - Implementation (TDD, quality)
- `orchestrator` - Task execution
- `refactor` - Behavior-preserving refactoring
- `coverage` - 100% test coverage
- `debugger` - Troubleshooting
- `security` - Security review
- `meta` - Agent system management

## Best Practices

**Skills:** Focused scope, actionable guidance, reference docs for depth.
**Personas:** Match a workflow, don't overload context.
**Commands:** Brief description, handle missing args.

## Efficiency Principles

**Core principle: Spend generously during preparation, save massively during execution.**

RPI phases have different characteristics:
- **Research/Plan/Authoring:** Happens ONCE, attended (human in loop) - spend freely to be thorough
- **Orchestration loop:** Happens N times, UNATTENDED (no human) - every byte multiplies, failures escalate

**Unattended execution is critical:** When `/orchestrate` runs, there's no human to provide missing context. Task files must be complete or sub-agents will fail, research (wasting tokens), or succeed partially (creating debt).

**Critical for execution efficiency:**

1. **Orchestrators load ZERO skills** - Pure execution loops only need their embedded logic
2. **Sub-agents read task files** - Never pass task content through orchestrator context
3. **Load skills on-demand** - Don't preload reference documentation "just in case"
4. **Personas are minimal** - Only skills needed for that specific workflow phase
5. **Reference vs working knowledge** - Large API docs go in separate skills, loaded when needed
6. **Complete task authoring** - Spend tokens researching during authoring so execution sub-agents don't need to

**Token budget awareness:** Orchestration loops can execute 10-50+ tasks. Every KB loaded per iteration multiplies across the entire pipeline. Spending 50K tokens during task authoring to save 2K per task execution = 50K invested, 100K saved (for 50 tasks).
