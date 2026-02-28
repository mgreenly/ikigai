Load a skillset (composite skill set) from `.claude/skillsets/`.

**Usage:**
- `/skillset NAME` - Load all skills defined in NAME.json

**Available skillsets:**
- `architect` - For architectural decisions (DDD, DI, patterns, naming, style)
- `developer` - For writing new code (TDD, style, naming, quality, coverage, zero-debt, jj)
- `meta` - For improving the .claude/ system
- `security` - For discovering security flaws

**Skillset JSON format:**
```json
{
  "preload": ["skill-a", "skill-b"],
  "advertise": [
    {"skill": "skill-c", "description": "One sentence description"}
  ]
}
```

- `preload`: Skills loaded immediately when skillset is activated
- `advertise`: Skills available on-demand (shown as reference, not loaded)

Skills are stored as directories in `.claude/library/` with a `SKILL.md` file.

---

{{#if args}}
Read `.claude/skillsets/{{args}}.json`

For each skill in `preload`: Read `.claude/library/<skill>/SKILL.md`

For `advertise`: Skills you can load with `/load <skill>` when you determine you need them.
{{else}}
**Available skillsets:**
- `architect` - For architectural decisions (DDD, DI, patterns, naming, style)
- `developer` - For writing new code (TDD, style, naming, quality, coverage, zero-debt, jj)
- `meta` - For improving the .claude/ system
- `security` - For discovering security flaws

Use `/skillset <name>` to load a skillset.
{{/if}}

Then wait for instructions.
