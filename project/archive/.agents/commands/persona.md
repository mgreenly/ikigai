Load a persona (composite skill set) from `.agents/personas/`.

**Usage:**
- `/persona NAME` - Load all skills defined in NAME.json

**Available personas:**
- `architect` - For architectural decisions (DDD, DI, patterns, naming, style)
- `coverage` - For achieving and maintaining 100% test coverage
- `debugger` - For debugging and troubleshooting issues
- `developer` - For writing new code (TDD, style, naming, quality, coverage, zero-debt, git)
- `meta` - For improving the .agents/ system and task framework
- `orchestrator` - For running tasks and orchestrating sub-agents via task system
- `researcher` - For research, user stories, and task authoring
- `security` - For discovering security flaws

**Persona JSON format:** Array of skill names/paths (without .md extension):
```
["default", "skill-a", "skill-b", "subdir/skill-c"]
```

Skills can reference subfolders for organized skill libraries.

**Action:** Read the persona JSON file and load all listed skills.

---

{{#if args}}
{{#each (split (cat ".agents/personas/{{args}}.json" | jq -r '.[]') "\n")}}
Read `.agents/skills/{{this}}.md`
{{/each}}
{{else}}
Error: Please specify a persona name (architect, coverage, debugger, developer, meta, orchestrator, researcher, or security)
{{/if}}

Then wait for instructions.
