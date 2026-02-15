---
name: mdp
description: Write mdp markdown banners for livestream display
---

# mdp

**mdp** is a terminal-based markdown presentation tool — it renders markdown files as full-screen slides in the terminal, navigated by keyboard.

## Purpose

Help write the livestream banner displayed during dev sessions. The banner lives at `../banner.md` (relative to project root).

## Banner Template

```markdown

-> **Ikigai - Dev Stream MM/DD/YY** <-

[[ THE BODY ]]

-> https://www.youtube.com/@IkigaiDevlog <-
-> https://ikigai.metaspot.org/ <-

```

- `-> text <-` centers text (mdp syntax)
- Update the date to today's date
- The body replaces `[[ THE BODY ]]`

## Body Rules

- **10 lines maximum.** Fewer is fine — don't pad.
- Content comes from the user's description of what they plan to work on
- Keep it concise and scannable — this is displayed on a livestream
- Plain markdown: short lines, no complex formatting

## Workflow

1. User describes the session topic or plan
2. Write the banner to `../banner.md` using the template above
3. Show the user what you wrote
