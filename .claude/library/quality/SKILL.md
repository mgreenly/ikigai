---
name: quality
description: Quality strategy — when and how to run checks
---

# Quality

Quality strategy for agents and humans. For check/fix script mechanics, see the `harness` skill.

## Pre-Commit Requirements

Before creating commits, run the 6 core quality checks (see `harness` skill for the full list and run order).

## Test Execution

**By Default**: Tests run in parallel, with 24 parallel tests on this machine.
- `MAKE_JOBS=24` - up to 24 concurrent tests

**When you need clear debug output** (serialize execution):
```bash
PARALLEL=0 MAKE_JOBS=1 make check
```

**Best practice**: Test individual files during development, run full suite before commits.

Example:
```bash
make build/tests/unit/array/basic_test && ./build/tests/unit/array/basic_test
```

## Build Modes

```bash
make BUILD={debug|release|sanitize|tsan|coverage}
```

- `debug` - Development builds with symbols
- `release` - Optimized production builds
- `sanitize` - Address and undefined behavior sanitizers
- `tsan` - Thread sanitizer
- `coverage` - Code coverage analysis

**CRITICAL**: Never run multiple `make` commands simultaneously. Different targets use incompatible compiler flags and will corrupt the build.
