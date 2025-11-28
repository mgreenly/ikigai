Load one or more skills from `.agents/skills/` into context.

**Usage:**
- `/load` - Load default.md
- `/load NAME` - Load NAME.md
- `/load NAME1 NAME2 ...` - Load multiple skills

**Arguments:** Space-separated skill names (without .md extension)

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
