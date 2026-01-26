# ikigai - Claude Code Context

## Project Context

* **ikigai** - Linux coding agent with terminal UI
* **Tech stack**: C, Linux, Makefile, PostgreSQL
* **VCS**: jj (Jujutsu) - a git-compatible version control system
* **Source**: `src/` (headers co-located with .c files)
* **Tests**: `tests/unit/`, `tests/integration/`, `tests/performance/`
* **Docs**: located in the `project/` directory (start with `project/README.md`)
* **ADRs**: `project/decisions/`

## Critical Rules

* **Never change directories** - Always stay in root, use relative paths
* **Never run parallel make** - Different targets use incompatible flags
* **Never use AskUserQuestion tool** - Forbidden in this project
* **Never use git commands** - This is a jj (Jujutsu) project; always use `jj` commands instead of `git`
* **Never merge to main locally** - All merges to main happen via GitHub PRs only
* **Never use background tasks** - Always run tasks in foreground unless explicitly asked
* **Never run ralph** - Never execute `.claude/harness/ralph/run`; the user runs ralph themselves

## Available Skills

Load these skills when you need deeper context:

* `/load memory` - talloc-based memory management and ownership rules
* `/load errors` - Result types with OK()/ERR() patterns
* `/load database` - PostgreSQL schema and query patterns
* `/load source-code` - Map of all src/*.c files by functional area
* `/load makefile` - Build targets, test commands, coverage requirements
* `/load jj` - jj workflow, commit policy, and permitted operations
* `/load notify` - Send push notifications to alert the user
* `/load task` - SQLite-backed task orchestration with escalation ladder
