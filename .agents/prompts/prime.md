# Prime

## Description
Prime the context with key contextual information about this project.

## Details

AGENTS.md is the primary agent memory file and should always be in context.

Don't enumerate or read the other files I list here unless you need to.

* This is "ikigai" - a multi-model coding agent with native terminal UI
* This is a "C" based "Linux" CLI project with a "Makefile"
* The source code is in src/
* The tests are in tests/unit, tests/integration and tests/performance
* The docs/ folder contains the project documentation
* The docs/README.md is the documentation hub - start there for details
* The docs/decisions folder contains "Architecture Decision Records"
* Memory: talloc-based with ownership rules (see docs/memory.md)
* Errors: Result types with OK()/ERR() patterns (see docs/error_handling.md)
* Use `make check` to verify tests while working on code changes
* Use `make lint && make coverage` before commits - 100% coverage is MANDATORY
* make BUILD={debug|release|sanitize|tsan|coverage} for different build modes
* you can use `sudo apt update`, `sudo apt upgrade` and `sudo apt install *`

**CRITICAL**: Never run multiple `make` commands simultaneously. Different targets use incompatible compiler flags and will corrupt the build.

## Naming Conventions

All public symbols follow: `ik_MODULE_THING`
- `ik_` - namespace prefix
- `MODULE` - single word (config, protocol, openai, handler)
- `THING` - descriptive name with approved abbreviations

Examples:
- `ik_cfg_load()` - function
- `ik_protocol_msg_t` - type
- `ik_httpd_shutdown` - global variable

Borrowed pointers use `_ref` suffix:
- `cfg_ref` - caller owns
- `manager_ref` - libulfius owns

Internal static symbols don't need `ik_` prefix.

See docs/naming.md for complete conventions and approved abbreviations.

## Test Code Style

Always add a blank line between END_TEST and START_TEST.

## Include Order

Follow Google C++ style guide for #include ordering:

1. Own header first (e.g., `config.h` in `config.c`) - catches missing dependencies
2. Project headers (`"header.h"`) - alphabetically sorted
3. System/library headers (`<header.h>`) - alphabetically sorted

Example:
```c
#include "config.h"           // Own header

#include "json_allocator.h"   // Project headers (alphabetical)
#include "logger.h"
#include "panic.h"
#include "wrapper.h"

#include <errno.h>            // System headers (alphabetical)
#include <stdlib.h>
#include <string.h>
```

Rationale: Project headers before system headers catches non-self-contained headers early.

## Pre-Commit Requirements

BEFORE creating ANY commit (mandatory, no exceptions):

1. `make fmt` - Format code
2. `make check` - ALL tests pass (100%)
3. `make lint` - ALL complexity/file size checks pass
4. `make coverage` - ALL metrics (lines, functions, branches) at 100.0%
5. `make check-dynamic` - ALL sanitizer checks pass (ASan, UBSan, TSan)

If ANY check fails: fix ALL issues, re-run ALL checks, repeat until everything passes.

Never commit with ANY known issue - even "pre-existing" or "in another file".

## Coverage

**CRITICAL:** Never change the LCOV_EXCL_COVERAGE value in the Makefile without explicit user permission.

Finding coverage gaps:
- `grep "^DA:" coverage/coverage.info | grep ",0$"` - uncovered lines
- `grep "^BRDA:" coverage/coverage.info | grep ",0$"` - uncovered branches

Coverage files:
- `coverage/coverage.info` - primary data source (parse with grep)
- `coverage/summary.txt` - human-readable summary
- Do NOT generate HTML reports (slow and unnecessary)

Coverage exclusions (LCOV markers):
- `LCOV_EXCL_START` / `LCOV_EXCL_STOP` - exclude blocks
- `LCOV_EXCL_LINE` - exclude specific lines
- `LCOV_EXCL_BR_LINE` - exclude branch coverage

## Test Execution

**By Default**: Tests run in parallel, with 24 parallel tests on this machine.
- `MAKE_JOBS=24` - up to 24 concurrent tests
- `PARALLEL=1` - all 4 check-dynamic subtargets in parallel

**When you need clear debug output** (serialize execution):
```bash
PARALLEL=0 MAKE_JOBS=1 PARALLEL=0 make check
PARALLEL=0 MAKE_JOBS=1 make check-valgrind
```

**Best practice**: Test individual files during development, run full suite before commits.

Example:
```bash
make build/tests/unit/array/basic_test && ./build/tests/unit/array/basic_test
```

## Git Configuration

- **Remote**: origin (github.com:mgreenly/ikigai.git)
- **Primary branch**: main
- **Upstream**: github/main

**Commit Policy:**

Do NOT include attributions:
- No "Co-Authored-By: Claude <noreply@anthropic.com>"
- No "🤖 Generated with [Claude Code](https://claude.com/claude-code)"
