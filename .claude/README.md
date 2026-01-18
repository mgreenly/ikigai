# .claude Directory

Agent infrastructure for the ikigai project. This directory contains the knowledge system, automation harnesses, and executable scripts that support Claude Code workflows.

## Architecture

The `.claude/` directory is organized around three core concepts:

1. **Knowledge system** - Skills and skillsets that provide domain context
2. **Automation system** - Harnesses and scripts that enforce quality gates
3. **Integration system** - Commands, hooks, and settings that bind to Claude Code

## Directory Structure

```
.claude/
├── commands/      # Slash command definitions
├── library/       # Knowledge modules (skills)
├── skillsets/     # Composite skill collections
├── scripts/       # Executable automation scripts
├── harness/       # Quality check automation loops
├── hooks/         # Event-triggered hooks
├── data/          # Runtime data (gitignored)
└── settings.json  # Claude Code configuration
```

**Quick links:** [commands/](commands/) • [library/](library/) • [skillsets/](skillsets/) • [scripts/](scripts/) • [harness/](harness/) • [hooks/](hooks/) • [settings.json](settings.json)

## Knowledge System

### [library/](library/)

Skills are knowledge modules. Each skill is a directory containing `SKILL.md` with domain-specific context.

**Meta perspective:** Skills solve the token efficiency problem. Instead of loading all project knowledge into every conversation, skills are loaded on-demand when their domain becomes relevant.

**Organization:**
- Flat structure: [library/database/SKILL.md](library/database/SKILL.md)
- Nested structure: [library/patterns/observer/SKILL.md](library/patterns/observer/SKILL.md)

**Loading:** Manual only via `/load <skill>` command. No auto-discovery.

**Examples to explore:**
- [database](library/database/SKILL.md) - PostgreSQL schema and query patterns
- [cdd](library/cdd/SKILL.md) - Context Driven Development pipeline
- [task](library/task/SKILL.md) - File-based task orchestration
- [tdd](library/tdd/SKILL.md) - Test-Driven Development workflow

### [skillsets/](skillsets/)

Skillsets are JSON files that bundle related skills for specific workflows.

**Meta perspective:** Skillsets map to workflow phases. Each phase needs different context - research needs internet facts, implementation needs coding patterns, debugging needs troubleshooting techniques. Skillsets provide phase-appropriate context bundles.

**Format:**
```json
{
  "preload": ["skill-a", "skill-b"],
  "advertise": [
    {"skill": "skill-c", "description": "..."}
  ]
}
```

- `preload`: Load immediately
- `advertise`: Reference only (load on-demand)

**Examples to explore:**
- [meta.json](skillsets/meta.json) - Agent system maintenance
- [planner.json](skillsets/planner.json) - Architecture and task breakdown
- [developer.json](skillsets/developer.json) - Feature development (TDD, style, naming, coverage)

## Automation System

### [scripts/](scripts/)

Executable TypeScript/shell scripts for task management and quality checks.

**Meta perspective:** Scripts are the executable layer. Skills provide knowledge, scripts provide action. Scripts manipulate state (task lifecycle), invoke tools (make targets), and enforce invariants (quality gates).

**Organization by function:**
- [scripts/task/](scripts/task/) - Task orchestration (init, start, done, fail, escalate)
- `scripts/check-*` - Quality check invocations (symlinks to harness runners)

**Examples to explore:**
- [scripts/task/init.ts](scripts/task/init.ts) - Initialize task directory structure
- [scripts/task/escalate.ts](scripts/task/escalate.ts) - Bump task capability level
- [scripts/task/next.ts](scripts/task/next.ts) - Get next pending task or stop
- [scripts/check-quality](scripts/check-quality) - Run all quality harnesses

### [harness/](harness/)

Automated quality check loops. Each harness runs a make target, spawns fix sub-agents on failure, and escalates on exhaustion.

**Meta perspective:** Harnesses enforce non-negotiable quality gates through mechanical iteration. They embody the principle "systems enforce standards" - no human willpower required, just run the loop until clean.

**Structure per harness:**
- `harness/<name>/run` - Executable runner script
- `harness/<name>/fix.prompt.md` - Fix agent instructions

**Examples to explore:**
- [harness/quality/](harness/quality/) - Orchestrate all quality checks
- [harness/sanitize/](harness/sanitize/) - Fix AddressSanitizer errors
- [harness/valgrind/](harness/valgrind/) - Fix Valgrind memory errors
- [harness/coverage/](harness/coverage/) - Enforce 80% per-file test coverage

## Integration System

### [commands/](commands/)

Slash command definitions. Markdown files with frontmatter + prompt template.

**Meta perspective:** Commands are the user-facing API to the automation system. They provide convenient invocation patterns for complex multi-step workflows.

**Format:**
```markdown
---
description: What the command does
---

Prompt template. Use {{args}} for arguments.
```

**Examples to explore:**
- [commands/load.md](commands/load.md) - Load skills from library
- [commands/skillset.md](commands/skillset.md) - Load composite skill sets
- [commands/update.md](commands/update.md) - Update auto-generated skills

### [hooks/](hooks/)

Event-triggered shell scripts that execute on Claude Code lifecycle events.

**Meta perspective:** Hooks are the integration seam between Claude Code's execution model and project-specific tooling. They allow injecting custom behavior without modifying Claude Code itself.

**Events:** session_start, user_prompt, pre_tool_use, post_tool_use, etc.

**Examples to explore:**
- [hooks/session_start.sh](hooks/session_start.sh) - Session initialization
- [hooks/user_prompt.sh](hooks/user_prompt.sh) - Before processing user input
- [hooks/post_tool_use.sh](hooks/post_tool_use.sh) - After tool execution

### [settings.json](settings.json)

Claude Code configuration binding hooks to events.

**Meta perspective:** Settings map the event namespace (Claude Code's lifecycle) to the handler namespace (this project's hooks). It's the wiring diagram.

### [data/](data/)

Persistent configuration data for automation workflows.

**Meta perspective:** Configuration state that workflows accumulate over time. Version controlled because it represents learned knowledge - false positive lists, known exceptions, curated exclusions that improve automation accuracy.

**Examples to explore:**
- [data/prune-false-positives.txt](data/prune-false-positives.txt) - Known false positives for dead code detection

## Design Philosophy

**Explicit over implicit:** No auto-discovery. Load skills manually. Opt-in, not opt-out.

**Minimal preload:** Skillsets load only what's necessary for their workflow. Token efficiency through discipline.

**Separation of concerns:** Knowledge (skills) separate from action (scripts) separate from invocation (commands).

**Mechanical enforcement:** Quality gates enforced by harnesses, not code review. Systems > willpower.

## Usage Patterns

**Research phase:** `/skillset researcher` → internet facts, codebase exploration

**Planning phase:** `/skillset planner` → architecture decisions, task breakdown

**Implementation phase:** `/skillset developer` → TDD, patterns, quality gates

**Quality enforcement:** `/check-quality` → run all harnesses until stable

**Meta work:** `/skillset meta` → modify this system itself

## Cross-References

- **CDD pipeline:** [library/cdd/SKILL.md](library/cdd/SKILL.md) - Release workflow details
- **Task orchestration:** [library/task/SKILL.md](library/task/SKILL.md) - Execution state tracking
- **Harness mechanics:** [library/harness/SKILL.md](library/harness/SKILL.md) - Quality loop implementation
- **Project principles:** [library/principles/SKILL.md](library/principles/SKILL.md) - Guiding philosophy
