---
description: Generate pseudo-code documentation for a source file
---

Generate pseudo-code documentation for: {{args}}

Use the Task tool with `model: "haiku"` and `subagent_type: "general-purpose"` to spawn a sub-agent that will:

1. Read the source file at {{args}}
2. Read the pseudo-code skill from `.claude/library/pseudo-code/SKILL.md` for style guidance
3. Determine target path: `project/pseudo-code/{{args}}.md`
4. Create parent directories if needed (use Bash: `mkdir -p`)
5. Generate markdown with:
   - `## Overview` - brief description of file's purpose
   - `## Code` - explanatory pseudo-code following the skill guidelines
6. Write the file using the Write tool

Include the full source file path and skill path in the prompt to the sub-agent.
