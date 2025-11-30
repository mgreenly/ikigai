Load one or more skills from `.agents/skills/` into context.

**Usage:**
- `/load` - Load default.md
- `/load NAME` - Load NAME.md
- `/load NAME1 NAME2 ...` - Load multiple skills
- `/load path/to/skill` - Load from subfolder

**Examples:**
- `/load tdd` - Load `.agents/skills/tdd.md`
- `/load patterns/observer` - Load `.agents/skills/patterns/observer.md`
- `/load di patterns/factory patterns/strategy` - Load multiple including subfolders

**Arguments:** Space-separated skill names/paths (without .md extension)

**Action:** Read the specified skill file(s) from `.agents/skills/`, then wait for instructions.

---

{{#if args}}
{{#each (split args " ")}}
Read `.agents/skills/{{this}}.md`
{{/each}}
{{else}}
Read `.agents/skills/default.md`
{{/if}}

Then wait for instructions.
