---
name: harness
description: Automated quality check loops with escalation and fix sub-agents
---

# Harness

Automated fix loops. Each harness runs a make target, spawns sub-agents to fix failures, commits on success, reverts on exhaustion.

**Pattern:** `.claude/harness/<name>/run` + `fix.prompt.md`

**Escalation:** sonnet:think → opus:think → opus:ultrathink

**History:** Each harness maintains `history.md` for cross-attempt learning. Truncated per-file, accumulates across escalation. Agents append summaries after each attempt so higher-level models can avoid repeating failed approaches.

## Quality Checks

The `check-*` namespace is reserved for quality checks. Only these scripts use the `check-` prefix. Each has a corresponding `fix-*` script that spawns sub-agents to fix failures.

### Core Quality Checks

The 6 core quality checks are the default exit gate for all work. Run these in order when work is complete.

| Script | What it verifies |
|--------|------------------|
| `check-compile` | Code compiles cleanly |
| `check-link` | Linker succeeds |
| `check-filesize` | File size under 16KB |
| `check-unit` | Unit tests pass |
| `check-integration` | Integration tests pass |
| `check-complexity` | Function complexity limits |

### Full Quality Suite

The full quality suite is the 6 core quality checks plus these 5 additional checks. Run only when explicitly requested.

| Script | What it verifies |
|--------|------------------|
| `check-sanitize` | Address/UB sanitizer clean |
| `check-tsan` | ThreadSanitizer clean |
| `check-valgrind` | Valgrind memcheck clean |
| `check-helgrind` | Valgrind helgrind clean |
| `check-coverage` | 90% line coverage met |

## Other Harnesses

Not part of the quality suite. Do not use the `check-*` prefix.

- `prune` — dead code detection (`.claude/scripts/prune`)
- `notify` — push notifications via ntfy.sh
- `pluribus` — multi-agent orchestration
- `reset-repo` — reset jj working copy to fresh state

## CLI

- `.claude/scripts/check-<name>` — symlinks to quality check harness run scripts
- `.claude/scripts/fix-<name>` — symlinks to fix harness run scripts
- `.claude/scripts/<name>` — symlinks for non-quality harnesses

## Running check-* Scripts

All check scripts are on PATH via `.claude/scripts/`.

- **Single file:** `check-compile --file=PATH` — scopes the check to one file. Use this during development for fast feedback.
- **Project-wide:** `check-compile` (no args) — checks everything. Use this as the exit gate when work is complete.
- **Timeout:** Use 60 minute timeout (`timeout: 3600000`)
- **Foreground:** Always run in foreground (never use `run_in_background`)
- **Blocking:** No output until completion — do not tail or monitor, just wait
- **Output format:** Structured JSON: `{"ok": true/false, "items": [...]}`
