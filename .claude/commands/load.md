Load one or more skills from `.claude/library/` into context.

**Usage:**
- `/load NAME` - Load NAME skill
- `/load NAME1 NAME2 ...` - Load multiple skills
- `/load path/to/skill` - Load from subfolder

**Examples:**
- `/load tdd` - Load `.claude/library/tdd/SKILL.md`
- `/load di memory errors` - Load multiple skills

**Arguments:** Space-separated skill names/paths (no .md extension)

**Notes:**
- For project context, see `CLAUDE.md` at the project root
- Loading a skill multiple times will re-read it (avoid if possible)

**Action:** Read the specified skill file(s) from `.claude/library/`, then wait for instructions.

---

{{#each (split args " ")}}
Read `.claude/library/{{this}}/SKILL.md`
{{/each}}

Then wait for instructions.
