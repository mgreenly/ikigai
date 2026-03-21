# Claude Code Instructions

Read `AGENTS.md` for project context, layout, quality harnesses, development workflow, and coding standards. This file adds Claude Code-specific tooling on top.

## .claude/ Directory

```
.claude/
├── commands/              # Slash command definitions
├── library/               # Skills (modular instruction sets)
├── skillsets/              # Composite skill bundles
├── harness/               # Quality check scripts
├── scripts/               # Symlinks to harness scripts (on PATH)
└── data/                  # Runtime data (gitignored)
```

## Goal Requirement

All content changes in this repository must be done through `/ralph-pipeline:ralph-goal` unless the user provides explicit instructions to do otherwise. Do not edit source files, tests, docs, or any other tracked content without a goal.

> **This file is an index, not an encyclopedia.** Each skill listed below is a self-contained document you load on demand with `/load <name>`. If you need to understand the database schema, load `database`. If you need error handling patterns, load `errors`. Don't assume information is missing just because it isn't inlined here — check the skill table first, then load what you need. Resist the urge to front-load everything; load skills relevant to your current task and trust that the detail is there when you follow the pointer.

## Skills

Use `/load <name>` to load a skill. Use `/load name1 name2` to load multiple.

| Skill | Description |
|-------|-------------|
| `memory` | talloc-based memory management and ownership rules |
| `errors` | Result types with OK()/ERR() patterns |
| `database` | PostgreSQL schema and query patterns |
| `source-code` | Map of all source files by functional area |
| `makefile` | Reference for modifying the Makefile (never run make directly — use check-* scripts) |
| `jj` | Jujutsu VCS workflow, commit policy, permitted operations |
| `style` | Code style conventions |
| `naming` | Naming conventions |
| `tdd` | Test-Driven Development workflow |
| `coverage` | 90% coverage requirement and exclusion rules |
| `lcov` | LCOV tooling for finding coverage gaps |
| `quality` | Quality checks — what to run and when |
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
| `harness` | Automated quality check loops with escalation |
| `scm` | Source code management workflow |
| `ctags` | Code navigation with ctags |
| `ikigai-ctl` | Control socket client for programmatic interaction with running ikigai |
| `notify` | Send push notifications via ntfy.sh |
| `headless` | Driving ikigai interactively from an agent session |
| `mdp` | Write mdp markdown banners for livestream display |
| `rules` | Critical project rules and constraints |
| `docs` | User documentation authoring guidelines |
| `bugs` | Known bugs and quirks in dependencies |
| `meta` | Agent system infrastructure (.claude/ directory) |
| `align` | Behavioral alignment rules |
| `principles` | Guiding principles and decision patterns |
| `scratch` | Temporary storage between sessions |
| `e2e-testing` | JSON-based end-to-end test format, runner, and mock provider |

### Skillsets

Use `/skillset <name>` to load a skillset.

| Skillset | Purpose |
|----------|---------|
| `developer` | Writing new code (TDD, style, quality, coverage, jj) |
| `architect` | Architectural decisions (DDD, DI, patterns, naming) |
| `security` | Discovering security flaws |
| `default` | Default skillset for all sessions |

## Claude Code-Specific Notes

- Run `/load harness` before using any harness scripts.
- Before modifying any `.c` or `.h` files, run `/load memory errors style naming ctags`.
- Full jj workflow: `/load jj`
- Full style guide: `/load style`
- ikigai-ctl protocol: `/load ikigai-ctl`
