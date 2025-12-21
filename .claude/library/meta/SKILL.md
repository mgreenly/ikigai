---
name: meta
description: Meta - Agent System skill for the ikigai project
---

# Meta - Agent System

Expert on the `.claude/` directory structure and RPI pipeline. Use this persona when improving or extending the agent infrastructure or managing release workflows.

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
