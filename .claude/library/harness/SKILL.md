---
name: harness
description: Automated quality check loops with escalation and fix sub-agents
---

# Harness

Automated fix loops in `.claude/harness/`. Each subdirectory wraps a `make` target, spawns Claude sub-agents to fix failures, and commits on success.

## Structure

```
.claude/harness/
├── check/       # make check (unit tests)
├── coverage/    # make coverage (line/branch coverage)
├── sanitize/    # make check-sanitize (ASan + UBSan)
├── tsan/        # make check-tsan (ThreadSanitizer)
├── valgrind/    # make check-valgrind (Memcheck)
├── helgrind/    # make check-helgrind (thread errors)
├── filesize/    # File size limits
├── complexity/  # Code complexity limits
└── quality/     # Runs all above in sequence
```

## How It Works

Each harness has a `run` script that:
1. Runs the make target
2. Parses failures
3. For each failure, spawns Claude sub-agent with `fix.prompt.md`
4. Escalates through ladder: sonnet:think → opus:think → opus:ultrathink
5. Commits on success, reverts on exhaustion
6. Loops until all pass or no progress

## Usage

```bash
.claude/harness/check/run      # Fix test failures
.claude/harness/coverage/run   # Fix coverage gaps
.claude/harness/quality/run    # Run all checks in sequence
```

## Quality Sequence

`quality/run` executes: filesize → check → complexity → sanitize → tsan → valgrind → helgrind → coverage
