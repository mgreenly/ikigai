# ikigai

Linux coding agent with terminal UI. Written in C, runs on Linux, uses PostgreSQL for persistence and direct terminal rendering for the UI.

## Architecture

Part of a multi-service system:

| Service | Language | Port | Purpose |
|---------|----------|------|---------|
| **ralph-plans** | Go + SQLite | 5001 | Goal storage and state machine |
| **ralph-runs** | Ruby | 5002 | Orchestrator + agent loop |
| **ralph-logs** | Go | 5003 | Real-time log streaming |
| **ralph-shows** | Deno + Preact | 5000 | Web UI dashboard |
| **ralph-counts** | Python | 5004 | Metrics dashboard |

## Critical Rules

- **Never change directories** - Always stay in root, use relative paths
- **Never run parallel make** - Different targets use incompatible flags
- **Never use AskUserQuestion tool** - Forbidden in this project
- **Never use git commands** - This is a jj (Jujutsu) project; always use `jj` commands instead of `git`
- **Never merge to main locally** - All merges to main happen via GitHub PRs only
- **Never use background tasks** - Always run tasks in foreground unless explicitly asked

## Source Layout

```
ikigai/
├── apps/ikigai/               # Main application source (C, headers co-located)
├── shared/                    # Shared libraries (error, logger, terminal, wrappers)
├── tests/
│   ├── unit/                  # Unit tests
│   ├── integration/           # Integration tests
│   └── helpers/               # Test utilities
├── project/                   # Docs, specs, ADRs (start with project/README.md)
│   └── decisions/             # Architecture Decision Records
├── scripts/
│   ├── bin/                   # Symlinks to goal scripts (on PATH)
│   └── goal-*/run             # Goal state management scripts (Ruby, return JSON)
├── .claude/
│   ├── commands/              # Slash command definitions
│   ├── library/               # Skills (modular instruction sets)
│   ├── skillsets/              # Composite skill bundles
│   ├── harness/               # Quality check + fix scripts
│   ├── scripts/               # Symlinks to harness scripts (on PATH)
│   └── data/                  # Runtime data (gitignored)
├── .envrc                     # direnv config (env vars, PATH)
└── AGENTS.md                  # This file
```

## Skills

Skills are modular instruction sets in `.claude/library/<name>/SKILL.md`.

- **Load a skill**: `/load <name>` reads the skill into context
- **Load multiple**: `/load name1 name2`

| Skill | Description |
|-------|-------------|
| `memory` | talloc-based memory management and ownership rules |
| `errors` | Result types with OK()/ERR() patterns |
| `database` | PostgreSQL schema and query patterns |
| `source-code` | Map of all source files by functional area |
| `makefile` | Build targets, test commands, coverage requirements |
| `jj` | Jujutsu VCS workflow, commit policy, permitted operations |
| `style` | Code style conventions |
| `naming` | Naming conventions |
| `tdd` | Test-Driven Development workflow |
| `tdd-strict` | TDD strict mode |
| `coverage` | 90% coverage requirement and exclusion rules |
| `lcov` | LCOV tooling for finding coverage gaps |
| `quality` | Quality checks and standards |
| `quality-strict` | Strict quality enforcement |
| `zero-debt` | Zero technical debt policy |
| `di` | Dependency injection patterns |
| `ddd` | Domain-Driven Design |
| `mocking` | Wrapper functions for test mocking |
| `testability` | Refactoring patterns for hard-to-test code |
| `debugger` | Debugging strategy and constraints |
| `gdbserver` | Remote debugging with gdbserver |
| `coredump` | Core dump analysis |
| `valgrind` | Memory error detection and leak checking |
| `sanitizers` | ASan/UBSan/TSan output interpretation |
| `debug-log` | Printf-style debug logging |
| `dev-dump` | Debug buffer dumps for terminal rendering state |
| `harness` | Automated quality check loops with escalation |
| `fix-checks` | Fix all quality check failures using ralph |
| `pipeline` | Goal creation and management via ralph-plans API |
| `goal-authoring` | Writing effective goal files for Ralph |
| `ralph` | External goal execution service |
| `pull-request` | Creating PRs with concise descriptions |
| `notify` | Send push notifications via ntfy.sh |
| `scm` | Source code management workflow |
| `ctags` | Code navigation with ctags |
| `event-log` | JSONL event stream for external integrations |
| `docs` | User documentation authoring guidelines |
| `bugs` | Known bugs and quirks in dependencies |
| `meta` | Agent system infrastructure (.claude/ directory) |
| `align` | Behavioral alignment rules |
| `principles` | Guiding principles and decision patterns |
| `scratch` | Temporary storage between sessions |

### Skillsets

Composite bundles in `.claude/skillsets/<name>.json`. Load with `/skillset <name>`.

```json
{
  "preload": ["skill-a"],
  "advertise": [{"skill": "skill-b", "description": "When to use"}]
}
```

- `preload` — loaded immediately when skillset is activated
- `advertise` — shown as available, loaded on demand with `/load`

| Skillset | Purpose |
|----------|---------|
| `developer` | Writing new code (TDD, style, quality, coverage, jj) |
| `implementor` | Base skillset for task execution (minimal: jj, errors, style, tdd) |
| `architect` | Architectural decisions (DDD, DI, patterns, naming) |
| `refactor` | Behavior-preserving code improvements |
| `debugger` | Debugging and troubleshooting |
| `coverage` | Achieving and maintaining 90% test coverage |
| `security` | Discovering security flaws |
| `orchestrator` | Running task execution loops (lean, no preloaded skills) |
| `meta` | Improving the .claude/ system |

### For Ralph

When Ralph executes a goal in this repo, it receives only `AGENTS.md` as project context. This file is responsible for getting Ralph everything it needs. Ralph uses `/load <skill>` to pull in deeper context on demand.

## Quality Harnesses

Check scripts detect issues; fix scripts attempt automated repair. All available on PATH via `.claude/scripts/`.

| Check | Fix | What it verifies |
|-------|-----|------------------|
| `check-compile` | `fix-compile` | Code compiles cleanly |
| `check-link` | `fix-link` | Linker succeeds |
| `check-unit` | `fix-unit` | Unit tests pass |
| `check-integration` | `fix-integration` | Integration tests pass |
| `check-sanitize` | `fix-sanitize` | Address/UB sanitizer clean |
| `check-tsan` | `fix-tsan` | ThreadSanitizer clean |
| `check-valgrind` | `fix-valgrind` | Valgrind memcheck clean |
| `check-helgrind` | `fix-helgrind` | Valgrind helgrind clean |
| `check-coverage` | `fix-coverage` | 90% line coverage met |
| `check-complexity` | `fix-complexity` | Function complexity limits |
| `check-filesize` | `fix-filesize` | File size under 16KB |
| `check-quality` | — | All checks combined |
| `check-prune` | `fix-prune` | Dead code detection |

Additional scripts: `notify` (push notifications), `pluribus` (parallel check runner), `reset-repo` (reset jj working copy).

Full guide: `/load harness`

## Goal Scripts

Goal management scripts live in `scripts/goal-*/run` (Ruby, return JSON). Symlinked from `scripts/bin/` and available on PATH via `.envrc`.

| Script | Purpose |
|--------|---------|
| `goal-create` | Create a new goal (draft). Requires `--org`/`--repo`. Body via stdin. |
| `goal-list` | List goals, optionally filtered by `--status` |
| `goal-get` | Get a single goal by ID |
| `goal-queue` | Queue a draft goal for execution |
| `goal-start` | Mark a goal as running |
| `goal-done` | Mark a goal as done |
| `goal-stuck` | Mark a goal as stuck |
| `goal-retry` | Retry a stuck goal |
| `goal-cancel` | Cancel a goal |
| `goal-comment` | Add a comment to a goal (body via stdin) |
| `goal-comments` | List comments on a goal |

### Creating a Goal

```bash
echo "## Objective
Implement feature X per project/plan/feature-x.md.

## Reference
- project/plan/feature-x.md

## Outcomes
- Feature X working
- Tests pass

## Acceptance
- All quality checks pass" | goal-create --org "$RALPH_ORG" --repo "$RALPH_REPO" --title "Implement feature X"
```

Then queue immediately: `goal-queue <id>`

### Goal Statuses

`draft` → `queued` → `running` → `done` (or `stuck` or `cancelled`)

Full guide: `/load goal-authoring`

## Development

### Tech Stack

- **C** (C11) with headers co-located alongside source
- **PostgreSQL** for persistence
- **talloc** for hierarchical memory management
- **Makefile** build system
- **jj** (Jujutsu) for version control

### Build & Test

```sh
make              # Build
make test-unit    # Run unit tests
make test-int     # Run integration tests
```

Full build reference: `/load makefile`

### Version Control

This project uses **jj** (Jujutsu), a git-compatible VCS. Never use git commands directly.

```sh
jj git fetch              # Fetch remote
jj new main@origin        # Start fresh on main
jj status                 # Check status
jj diff                   # View changes
jj log                    # View log
jj commit -m "msg"        # Commit
```

Full jj workflow: `/load jj`

### Code Style

- C11, no compiler extensions
- Headers co-located with .c files
- `snake_case` for functions and variables
- Typed errors via Result pattern: `OK()` / `ERR()`
- talloc for all heap allocation (hierarchical ownership)
- 16KB max file size — split before it hurts

Full style guide: `/load style`

### Environment

Configured via `.envrc` (direnv). PATH includes `scripts/bin/` for goal scripts and `.claude/scripts/` for harness scripts. Services communicate via `RALPH_*` env vars.
