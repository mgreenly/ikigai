# .claude Directory

Claude Code configuration and skills for the ikigai project. This directory is completely independent of `.agents/` and `.ikigai/`.

## Structure

```
.claude/
├── commands/      # Slash commands for manual invocation
├── data/          # Runtime data (gitignored)
├── hooks/         # Event-triggered hooks
├── library/       # Skill library (manual load only via /load)
├── skillsets/     # Composite skill sets
└── settings.json  # Claude Code configuration
```

## Key Concepts

### Auto-Discovery vs Manual Loading

**Auto-Discovery (Disabled):**
- Claude Code auto-discovers skills from `.claude/skills/`
- We keep this directory empty to prevent automatic context loading
- Minimizes token usage and gives explicit control

**Manual Loading:**
- All skills stored in `.claude/library/`
- Use `/load <skill>` to explicitly load when needed
- Only loads what you request, when you request it

## Commands

### `/load [skill...]`

Load one or more skills from the library.

**Examples:**
```bash
/load                          # Load default skill
/load tdd                      # Load TDD skill
/load patterns/observer        # Load nested skill
/load tdd database mocking     # Load multiple skills
```

### `/skillset <name>`

Load a skillset (composite skill set).

**Available skillsets:**
- `architect` - Architectural decisions (DDD, DI, patterns)
- `coverage` - Test coverage enforcement
- `debugger` - Debugging workflows
- `developer` - Feature development
- `meta` - .claude/ system maintenance
- `orchestrator` - Task orchestration
- `refactor` - Code refactoring
- `researcher` - Research and planning
- `security` - Security review

### `/update-skills`

Update auto-generated skills by analyzing the codebase.

**Updates:**
- `source-code` - Map of src/*.c files
- `makefile` - Make targets reference
- `database` - Schema and database API
- `mocking` - MOCKABLE wrappers
- `errors` - Error codes enum

## Skills

### Format

All skills use the directory format:

```
.claude/library/skill-name/
└── SKILL.md
```

**SKILL.md structure:**
```yaml
---
name: skill-name
description: What this skill does and when to use it
---

# Skill Title

## Section
Content...
```

### Organization

**Top-level skills:** `.claude/library/<skill>/SKILL.md`
- `default`, `database`, `tdd`, `mocking`, etc.

**Nested skills:** `.claude/library/<category>/<skill>/SKILL.md`
- `patterns/observer`, `patterns/factory`
- `security/memory-safety`, `security/input-validation`
- `refactoring/smells`, `refactoring/techniques`

**Special skill:** `task/` includes TypeScript implementation files for the task database system. The database itself is stored at `.claude/data/tasks.db` (gitignored).

### Loading Nested Skills

```bash
/load patterns/observer        # Load specific pattern
/load security/memory-safety   # Load security skill
/load refactoring/techniques   # Load refactoring skill
```

## Skillsets

Skillsets are JSON arrays listing skills to load together.

**Format:**
```json
[
  "default",
  "skill-a",
  "patterns/skill-b"
]
```

**Example (`developer.json`):**
```json
[
  "default",
  "tdd",
  "style",
  "naming",
  "quality",
  "coverage",
  "zero-debt",
  "jj"
]
```

All skills are loaded from `.claude/library/` - no special handling for nested paths.

## Hooks

Event-triggered shell scripts in `.claude/hooks/`:

- `session_start.sh` - Session initialization
- `session_end.sh` - Session cleanup
- `user_prompt.sh` - Before processing user input
- `stop.sh` - Agent stop
- `subagent_stop.sh` - Sub-agent stop
- `pre_tool_use.sh` - Before tool execution
- `post_tool_use.sh` - After tool execution
- `permission_request.sh` - Permission requests
- `notification.sh` - Notifications
- `pre_compact.sh` - Before context compaction

Hooks are configured in `settings.json`.

## Settings

`settings.json` configures hook bindings.

**Example:**
```json
{
  "hooks": {
    "SessionStart": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": ".claude/hooks/session_start.sh",
            "timeout": 30
          }
        ]
      }
    ]
  }
}
```

## Migration Notes

This directory was migrated from `.agents/` to create separation between:
- **`.claude/`** - Claude Code specific infrastructure
- **`.agents/`** - Legacy/alternative agent system
- **`.ikigai/`** - ikigai agent infrastructure

All skills converted from flat `.md` files to directory format with YAML frontmatter.

## Best Practices

1. **Keep auto-discovery empty** - Don't create `.claude/skills/` to avoid automatic loading
2. **Use `/load` explicitly** - Manual loading gives you control
3. **Organize by category** - Use nested paths for related skills
4. **Update skill frontmatter** - Ensure `name` and `description` fields are accurate
5. **Use skillsets for workflows** - Bundle commonly-used skills together
6. **Keep skills focused** - One domain per skill, concise content

## Quick Reference

**Load default context:**
```bash
/load
```

**Load specific skill:**
```bash
/load database
```

**Load developer skillset:**
```bash
/skillset developer
```

**Update auto-generated skills:**
```bash
/update-skills
```
