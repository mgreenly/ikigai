# Meta - Agent System

Expert on the `.agents/` directory structure. Use this persona when improving or extending the agent infrastructure.

## Directory Structure

```
.agents/
├── commands/    # Slash command definitions
├── hooks/       # Event-triggered shell scripts
├── logs/        # Agent activity logs
├── personas/    # Composite skill sets (JSON)
└── skills/      # Knowledge modules (Markdown)
```

## Skills (`.agents/skills/`)

Markdown files providing focused context and guidance. Loaded via `/load` or as part of personas.

**Format:**
```markdown
# Skill Name

One-sentence description of what this skill provides.

## Section
Focused guidance, patterns, checklists.
```

**Conventions:**
- One domain per skill (separation of concerns)
- Keep concise (~20-100 lines)
- Subdirectories for related groups: `patterns/`, `security/`
- Reference other skills/docs when needed

## Commands (`.agents/commands/`)

Markdown files defining slash commands with Handlebars templating.

**Format:**
```markdown
Description and usage instructions.

---

{{#if args}}
Action with {{args}}
{{else}}
Error: Please provide arguments
{{/if}}

Then wait for instructions.
```

**Template features:**
- `{{args}}` - Raw arguments string
- `{{#if args}}` - Conditional on arguments
- `{{#each (split args " ")}}` - Loop over space-separated args
- `{{cat "file.json" | jq '.field'}}` - Shell commands

## Personas (`.agents/personas/`)

JSON arrays listing skills to load together for a specific role.

**Format:**
```json
[
  "default",
  "skill-a",
  "skill-b",
  "subdir/skill-c"
]
```

**Conventions:**
- Include `default` first (project context)
- Alphabetical order after default
- 5-15 skills per persona
- Name reflects role: `developer`, `coverage`, `security`

**Current personas:**
- `architect` - Design patterns, DDD, DI
- `coverage` - 100% test coverage (strict)
- `developer` - Feature development (momentum)
- `meta` - Agent system itself
- `security` - Security review and threat modeling

## Hooks (`.agents/hooks/`)

Shell scripts triggered by agent events. Named by event type.

**Events:**
- `pre-commit` - Before git commits
- `post-tool` - After tool execution
- Custom events as needed

**Format:** Executable shell scripts with appropriate shebang.

## Logs (`.agents/logs/`)

Activity logs for debugging and auditing agent behavior.

## Best Practices

**Creating skills:**
- Clear one-sentence description
- Focused scope (single domain)
- Actionable guidance over theory
- Reference detailed docs for deep dives

**Creating personas:**
- Match a specific workflow or role
- Don't overload (context window limits)
- Test skill combinations work together

**Creating commands:**
- Brief description with usage examples
- Handle missing arguments gracefully
- End with "Then wait for instructions"

## Extending the System

1. **Skill** - Add knowledge or procedures
2. **Command** - Add user-invokable shortcut
3. **Persona** - Combine skills for a role

**Naming:** kebab-case for all files (`my-skill.md`, `my-command.md`)
