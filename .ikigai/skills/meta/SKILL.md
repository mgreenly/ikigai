---
name: meta
description: Meta - Agent System infrastructure for the ikigai project
---

# Meta - Agent System

Expert on the `.ikigai/` directory structure and agent infrastructure. Use this skillset when improving or extending the agent system, skills, skillsets, or commands.

## Directory Structure

```
.ikigai/
├── debug/     # runtime debug file dumps
├── data/      # runtime data
├── logs/      # runtime logs
├── scripts/   # cli scripts on path
└── skills/    # Knowledge modules
```

## Skills (`.ikigai/library/`)

Each skill is a directory containing `SKILL.md`.

These files are typically are pinned (/pin) and are used to compse the system prompt.

Don't load unless asked.

**Conventions:**
- One domain per skill
- Keep concise (~20-100 lines)

**Skill structure:**

```markdown
---
name: skill-name
description: Brief description
---

# Skill Name

Content here...
```
## Best Practices

**Skills:**
- Focused scope, single domain
- Actionable guidance over theory
- Reference docs for depth, load on demand
- Both mechanical (how) and conceptual (why) layers
