---
name: quality
description: Quality checks — what to run and when
---

# Quality

## Core Checks (default exit gate)

Six checks run project-wide, in order, once when work is complete. This is the default meaning of "all checks pass."

`check-compile` → `check-link` → `check-filesize` → `check-unit` → `check-integration` → `check-complexity`

If any fail, fix and re-run only the failing check.

## Full Suite (only when explicitly requested)

The core 6 plus 5 deep checks. Run only when the goal or user specifically asks.

`check-sanitize` · `check-tsan` · `check-valgrind` · `check-helgrind` · `check-coverage`

## Development Inner Loop

After changing a file, run the relevant check with `--file=PATH`:

- `check-compile --file=PATH` after every edit
- `check-unit --file=PATH` when a test file exists

Stay in this single-file loop. Do not run project-wide checks during active development.

## Build Modes

`make BUILD={debug|release|sanitize|tsan|coverage}`

**CRITICAL**: Never run multiple `make` commands simultaneously — incompatible compiler flags corrupt the build.