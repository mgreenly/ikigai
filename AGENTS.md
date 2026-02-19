# ikigai

Ikigai is a terminal-based coding agent written in C for Linux, similar in purpose to Claude Code but designed as a modular, experimental platform. It supports multiple AI providers (Anthropic, OpenAI, Google) and records all user and LLM messages as events in PostgreSQL, creating a permanent conversation history that outlives any single context window. Agents are organized in an unbounded process tree — users create long-lived agents with custom system prompts built from stacked pinned documents, and those agents can spawn temporary child agents as needed. The flagship feature, currently in development, is a sliding context window: you set a token budget, old messages fall off the back, and a reserved portion is filled with automatically generated summaries drawn from the complete database history. The codebase is built to production standards but structured for experimentation — modular and plug-and-play so new ideas can be tested without destabilizing what works.

> **This file is an index, not an encyclopedia.** It tells you where to find information, not the information itself. Each skill listed below is a self-contained document you load on demand with `/load <name>`. If you need to understand the database schema, load `database`. If you need error handling patterns, load `errors`. Don't assume information is missing just because it isn't inlined here — check the skill table first, then load what you need. Resist the urge to front-load everything; load skills relevant to your current task and trust that the detail is there when you follow the pointer.

## Project Layout

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

Use `/load <name>` to load a skill. Use `/load name1 name2` to load multiple.

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
| `ddd` | Domain vocabulary, bounded contexts, core entities and invariants |
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
| `fix-checks` | Fix all quality check failures using Ralph |
| `pipeline` | Goal creation and management via ralph-plans API |
| `goal-authoring` | Writing effective goal files for Ralph |
| `ralph` | Ralph agent loop and configuration |

> **Ralph** is a simple autonomous agent loop: run a prompt, inspect progress, repeat until the goal is complete. Each iteration's progress output and file changes carry context forward. The `ralph-runs` service orchestrates multiple Ralph instances, distributing goals written via `goal-authoring` and submitted through `pipeline`. Load `ralph` before interacting with any of these.
| `pull-request` | Creating PRs with concise descriptions |
| `scm` | Source code management workflow |
| `ctags` | Code navigation with ctags |
| `event-log` | JSONL event stream for external integrations |
| `ikigai-ctl` | Control socket client for programmatic interaction with running ikigai |
| `docs` | User documentation authoring guidelines |
| `bugs` | Known bugs and quirks in dependencies |
| `meta` | Agent system infrastructure (.claude/ directory) |
| `align` | Behavioral alignment rules |
| `principles` | Guiding principles and decision patterns |
| `scratch` | Temporary storage between sessions |
| `end-to-end-testing` | JSON-based end-to-end test format for mock and live providers |

### Skillsets

Use `/skillset <name>` to load a skillset.

| Skillset | Purpose |
|----------|---------|
| `developer` | Writing new code (TDD, style, quality, coverage, jj) |
| `implementor` | Base skillset for task execution (minimal: jj, errors, style, tdd) |
| `architect` | Architectural decisions (DDD, DI, patterns, naming) |
| `refactor` | Behavior-preserving code improvements |
| `debugger` | Debugging and troubleshooting |
| `security` | Discovering security flaws |
| `orchestrator` | Running task execution loops (lean, no preloaded skills) |
| `meta` | Improving the .claude/ system |

## Quality Harnesses

Run `/load harness` before using any harness scripts. Never run `make` targets directly — the check scripts are the only interface. All check scripts accept `--file=PATH` to check a single file. All scripts are on PATH via `.claude/scripts/`.

### Development Inner Loop (while coding)

After changing a file, run the relevant check with `--file=PATH` on that file:

- `check-compile --file=PATH` after every edit
- `check-unit --file=PATH` when a test file exists
- Other checks as relevant to the change

Stay in this single-file loop. It takes seconds. Do not run project-wide checks during active development.

### Standard Checks (exit gate — run once when work is complete)

Six checks run project-wide in order, once, when all changes are done:

| Check | Fix | What it verifies |
|-------|-----|------------------|
| `check-compile` | `fix-compile` | Code compiles cleanly |
| `check-link` | `fix-link` | Linker succeeds |
| `check-filesize` | `fix-filesize` | File size under 16KB |
| `check-unit` | `fix-unit` | Unit tests pass |
| `check-integration` | `fix-integration` | Integration tests pass |
| `check-complexity` | `fix-complexity` | Function complexity limits |

This is the default meaning of "all checks pass" in any goal. If any fail, fix and re-run only the failing check, not all six.

### Deep Checks (only when explicitly requested)

Five additional checks run only when the goal or user explicitly asks for them:

| Check | Fix | What it verifies |
|-------|-----|------------------|
| `check-sanitize` | `fix-sanitize` | Address/UB sanitizer clean |
| `check-tsan` | `fix-tsan` | ThreadSanitizer clean |
| `check-valgrind` | `fix-valgrind` | Valgrind memcheck clean |
| `check-helgrind` | `fix-helgrind` | Valgrind helgrind clean |
| `check-coverage` | `fix-coverage` | 90% line coverage met |


## Running the Application

**You must use `--headless` mode.** Without it, ikigai renders to the alternate terminal buffer — no visible output, no way to interact from an agent session.

```bash
bin/ikigai --headless &          # Start in background (no TTY required)
ikigai-ctl read_framebuffer      # Read screen contents
ikigai-ctl send_keys "hello\r"   # Send keystrokes (\r = Enter)
```

See `/load ikigai-ctl` for full protocol. Socket auto-discovered from `run/`. Kill the process when done.

## Development

Before modifying any `.c` or `.h` files, run `/load memory errors style naming ctags`.

### Testing Workflow

**While coding** — use single-file checks with `--file=PATH`:

```sh
check-compile --file=apps/ikigai/parser.c    # After every edit
check-unit --file=tests/unit/parser_test.c   # When test file exists
```

This is your inner loop. Stay here. Do not run project-wide checks during active development.

**When work is complete** — run the six standard checks project-wide, in order:

```sh
check-compile
check-link
check-filesize
check-unit
check-integration
check-complexity
```

If any fail, fix and re-run only the failing check. These six checks are the default exit gate for all work.

**Deep checks** — run only when explicitly requested:

```sh
check-sanitize      # Only when goal asks for sanitizer checks
check-tsan          # Only when goal asks for thread safety verification
check-valgrind      # Only when goal asks for memory analysis
check-helgrind      # Only when goal asks for thread error detection
check-coverage      # Only when goal asks for coverage verification
```

### Tech Stack

- **C** (C11) with headers co-located alongside source
- **PostgreSQL** for persistence
- **talloc** for hierarchical memory management
- **jj** (Jujutsu) for version control

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
