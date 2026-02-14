---
name: rules
description: Critical project rules and constraints
---

# Critical Rules

- **Never change directories** - Always stay in root, use relative paths
- **Never use AskUserQuestion tool** - Forbidden in this project
- **Never use git commands** - This is a jj (Jujutsu) project; always use `jj` commands instead of `git`
- **Never merge to main locally** - All merges to main happen via GitHub PRs only
- **Never use background tasks** - Always run tasks in foreground unless explicitly asked
