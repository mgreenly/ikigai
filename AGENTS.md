# ikigai

Ikigai is a terminal-based coding agent written in C for Linux, similar in purpose to Claude Code but designed as a modular, experimental platform. It supports multiple AI providers (Anthropic, OpenAI, Google) and records all user and LLM messages as events in PostgreSQL, creating a permanent conversation history that outlives any single context window. Agents are organized in an unbounded process tree — users create long-lived agents with custom system prompts built from stacked pinned documents, and those agents can spawn temporary child agents as needed. The flagship feature, currently in development, is a sliding context window: you set a token budget, old messages fall off the back, and a reserved portion is filled with automatically generated summaries drawn from the complete database history. The codebase is built to production standards but structured for experimentation — modular and plug-and-play so new ideas can be tested without destabilizing what works.

**Project start date:** 2025-11-01

## Goal Requirement

All content changes in this repository must be done through goals unless the user provides explicit instructions to do otherwise. Do not edit source files, tests, docs, or any other tracked content without a goal.

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
├── .envrc                     # direnv config (env vars, PATH)
└── AGENTS.md                  # This file
```

## Development

### Tech Stack

- **C** (C11) with headers co-located alongside source
- **PostgreSQL** for persistence
- **talloc** for hierarchical memory management
- **jj** (Jujutsu) for version control

### Version Control

**"All files" means ALL files.** When the user says to commit, push, restore, or rebase "all files", that means every modified file in the working copy — no exceptions. Never selectively exclude files based on your own judgment about which changes are "relevant" or "from this session." The working copy is the source of truth.

This project uses **jj** (Jujutsu), a git-compatible VCS. Never use git commands directly.

```sh
jj git fetch              # Fetch remote
jj new main@origin        # Start fresh on main
jj status                 # Check status
jj diff                   # View changes
jj log                    # View log
jj commit -m "msg"        # Commit
```

### Code Style

- C11, no compiler extensions
- Headers co-located with .c files
- `snake_case` for functions and variables
- Typed errors via Result pattern: `OK()` / `ERR()`
- talloc for all heap allocation (hierarchical ownership)
- 16KB max file size — split before it hurts

### Environment

Configured via `.envrc` (direnv). PATH includes goal and harness scripts. Services communicate via `RALPH_*` env vars.
