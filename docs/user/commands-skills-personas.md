# Commands, Skills, and Personas

Ikigai's behavior can be customized through three mechanisms: **commands**, **skills**, and **personas**.

## Overview

| Type | Purpose | Invocation |
|------|---------|------------|
| Command | Slash commands that trigger actions | `/command-name` |
| Skill | Knowledge and instructions loaded into context | `/skill skill-name` |
| Persona | Bundles of skills for specific roles | `/persona persona-name` |

## Where They Live

Ikigai looks for these in three locations, in order of priority:

| Priority | Location | Purpose |
|----------|----------|---------|
| 1 | `.agents/` | Project-specific customizations |
| 2 | `~/.config/ikigai/` | Your personal defaults |
| 3 | Built-in | Ships with Ikigai |

The first match wins. If you create `.agents/personas/architect.json`, it overrides the built-in `architect` persona.

### Directory Structure

Each location uses the same structure:

```
commands/
  my-command.md
skills/
  my-skill.md
  subdir/
    another-skill.md
personas/
  my-persona.json
```

## Commands

Commands are markdown files that define slash command behavior.

**Location:** `commands/command-name.md`

**Usage:** `/command-name [arguments]`

### Example

`.agents/commands/review.md`:
```markdown
Review the code changes in the current branch.

Focus on:
- Correctness
- Performance implications
- Test coverage
```

Invoke with: `/review`

## Skills

Skills inject knowledge and instructions into the conversation context.

**Location:** `skills/skill-name.md`

**Usage:** `/skill skill-name`

Skills can be organized in subdirectories:
- `skills/patterns/factory.md` → `/skill patterns/factory`

### Example

`.agents/skills/house-style.md`:
```markdown
# House Style

- Use snake_case for functions and variables
- Prefix all public functions with `ik_`
- Keep functions under 50 lines
```

## Personas

Personas bundle multiple skills into a single loadable profile.

**Location:** `personas/persona-name.json`

**Usage:** `/persona persona-name`

### Format

A persona is a JSON array of skill names:

```json
["default", "ddd", "patterns/factory", "my-custom-skill"]
```

### Extending Built-ins

To extend a built-in persona rather than replace it entirely, reference built-in skills explicitly:

```json
["builtin:default", "builtin:ddd", "my-team-conventions"]
```

## Accessing Overridden Items

When you override a built-in, the original remains accessible via namespace prefix:

| Prefix | Source |
|--------|--------|
| `builtin:` | Built-in (ships with Ikigai) |
| `global:` | User global (`~/.config/ikigai/`) |
| (none) | Priority resolution (project → global → built-in) |

### Examples

```
/persona architect           # Uses your override if present
/persona builtin:architect   # Always uses built-in
/persona global:architect    # Always uses ~/.config/ikigai/ version
```

This also works in persona JSON files:

```json
["builtin:default", "global:team-style", "project-specific"]
```

## Quick Start

1. Create a project-local customization directory:
   ```
   mkdir -p .agents/skills
   ```

2. Add a skill:
   ```
   echo "# My Skill\n\nCustom instructions here." > .agents/skills/my-skill.md
   ```

3. Load it:
   ```
   /skill my-skill
   ```

## Tips

- **Start by copying**: Use `/skill show builtin:skill-name` to see a built-in's content, then customize
- **Project-local first**: Put project-specific conventions in `.agents/`
- **Global for personal defaults**: Put your personal preferences in `~/.config/ikigai/`
- **Compose, don't replace**: Use `builtin:` references in personas to extend rather than fully replace
