# ikigai - Claude Code Context

## Project Context

* **ikigai** - Multi-model coding agent with native terminal UI
* **Tech stack**: C, Linux, Makefile, PostgreSQL (libpq)
* **Source**: `src/` (headers co-located with .c files)
* **Tests**: `tests/unit/`, `tests/integration/`, `tests/performance/`
* **Docs**: `project/` (start with `project/README.md`)
* **ADRs**: `project/decisions/`

## Critical Rules

* **Never change directories** - Always stay in root, use relative paths
* **Never run parallel make** - Different targets use incompatible flags
* **Never use AskUserQuestion tool** - Forbidden in this project

## Available Skills

Load these skills when you need deeper context:

* `/load memory` - talloc-based memory management and ownership rules
* `/load errors` - Result types with OK()/ERR() patterns
* `/load database` - PostgreSQL schema and query patterns
* `/load source-code` - Map of all src/*.c files by functional area
* `/load makefile` - Build targets, test commands, coverage requirements
* `/load git` - Commit policy and permitted git operations
