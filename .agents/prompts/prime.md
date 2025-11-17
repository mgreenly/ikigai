# Prime

## Description
Prime the context with key contextual information about the this project.

## Details

AGENT.md is the primary agent memory file and should always be in context.

Don't enumerate or read the other files I list here unless you need to.

* This is a "C" based "Linux" CLI project it has a "Makefile".
* The source code is in src/
* The tests are in tests/unit, tests/integration and tests/performance
* The docs/ folder contains the project documentation.
* The docs/README.md contains a project summary and plan.
* The docs/decisions folder contains "Architecture Decisions Records"
* Use `make check` to verify tests while working on code changes.
* Use `make lint && make coverage` before making commits to verify meets quality thresholds.
