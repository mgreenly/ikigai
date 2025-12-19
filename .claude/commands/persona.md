Load a persona (composite skill set) from `.claude/personas/`.

**Usage:**
- `/persona NAME` - Load all skills defined in NAME.json

**Available personas:**
- `architect` - For architectural decisions (DDD, DI, patterns, naming, style)
- `coverage` - For achieving and maintaining 100% test coverage
- `debugger` - For debugging and troubleshooting issues
- `developer` - For writing new code (TDD, style, naming, quality, coverage, zero-debt, git)
- `meta` - For improving the .claude/ system
- `security` - For discovering security flaws

**Persona JSON format:** Array of skill names/paths:
```
["default", "skill-a", "skill-b", "subdir/skill-c"]
```

Skills are stored as directories in `.claude/library/` with a `SKILL.md` file.

**Action:** Read the persona JSON file and load all listed skills.

---

{{#if args}}
{{#each (split (cat ".claude/personas/{{args}}.json" | jq -r '.[]') "\n")}}
Read `.claude/library/{{this}}/SKILL.md`
{{/each}}
{{else}}
Error: Please specify a persona name (architect, coverage, debugger, developer, meta, or security)
{{/if}}

Then wait for instructions.
