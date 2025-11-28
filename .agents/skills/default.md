# Default

## Description
Load default context with key information about this project.

## Details

AGENTS.md is the primary agent memory file and should always be in context.

Don't enumerate or read the other files I list here unless you need to.

* This is "ikigai" - a multi-model coding agent with native terminal UI
* This is a "C" based "Linux" CLI project with a "Makefile"
* The source code is in src/
* Header files (*.h) ALWAYS exist in the same directory as their (*.c) files
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

## Scripts

* Each script directory has `README.md` with: exact command (including Deno permissions), arguments table, JSON output format
* All scripts return JSON: `{success: bool, data: {...}}` on success, `{success: false, error: string, code: string}` on error
* Use `jq` and coreutils to manipulate JSON results
* See docs/agent-scripts.md for architecture details
