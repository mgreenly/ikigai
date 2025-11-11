# Agent Notes

docs/README.md provides an overview of the project.

## Core Principles

**0. NEVER**
1. Never change LCOV_EXCL_COVERAGE"

**1. Test-Driven Development (TDD)**

**ABSOLUTE RULE: NEVER WRITE CODE BEFORE YOU HAVE A TEST THAT NEEDS IT**

Follow strict Red/Green/Verify cycle:

1. **Red**: Write a failing test first
   - Write the test code that calls the new function
   - Add function declaration to header file
   - Add stub implementation that compiles but does nothing (e.g., `return OK(NULL);`)
   - **IMPORTANT**: A compilation error is NOT a failing test - you need a stub that compiles and runs
   - Verify the test actually fails with assertion failures (e.g., wrong output)
   - NO CODE exists until a test demands it

2. **Green**: Write minimal code to make the test pass
   - Implement ONLY what the test requires
   - STOP immediately when the test passes
   - DO NOT write "helper functions" before they're called
   - DO NOT write code "because you'll need it later"
   - DO NOT refactor for complexity until `make lint` actually fails

3. **Verify**: Run quality checks
   - `make check` - All tests must pass
   - `make lint` - Code complexity under threshold

**WARNING**: Writing code before tests wastes tokens, time, and money by:
- Generating premature code
- Debugging unnecessary coverage gaps
- Reverting unused code
- Violating the core methodology

**The test MUST come first. No exceptions.**

If writing a helper function, ask: "Does a passing test call this right now?" If no, DELETE IT.

**2. Zero Technical Debt**

When you discover any problem, inconsistency, or standards violation:

1. Fix it immediately - don't defer or document
2. Fix it completely - address root cause, not symptoms
3. Fix it systematically - search for similar issues elsewhere

Examples requiring immediate fixes:
- Missing assertions on function preconditions
- Inconsistent naming conventions
- Code violating error handling philosophy
- Test coverage gaps
- Code not following established patterns

**When to ask first:**
- Architectural changes
- Unclear which solution is correct
- Breaking changes to public APIs
- Uncertainty about project conventions

If you discover deficiencies in existing code while working nearby, fix them as part of your current work.

**3. Work Style**

**CRITICAL**: Do not make architectural, structural, or refactoring changes unless explicitly requested.

Acceptable without asking:
- Formatting with `make fmt`
- Running `make check` and `make lint`
- Minor code style consistency

When discussing improvements:
- Present options and trade-offs
- Wait for explicit approval before implementing

## Quality Standards

**Pre-Commit Requirements**

BEFORE creating ANY commit (mandatory, no exceptions):

1. `make fmt` - Format code
2. `make check` - ALL tests pass (100%)
3. `make lint` - ALL complexity/file size checks pass
4. `make coverage` - ALL metrics (lines, functions, branches) at 100.0%
5. `make check-dynamic` - ALL sanitizer checks pass (ASan, UBSan, TSan)

If ANY check fails: fix ALL issues, re-run ALL checks, repeat until everything passes.

**Never commit with ANY known issue - even "pre-existing" or "in another file".**

**Coverage Requirements**

**CRITICAL: 100% coverage of Lines, Functions, and Branches for the ENTIRE codebase.**

The requirement is ABSOLUTE:
- 100% of ALL code, not just new code
- A single uncovered line or branch in ANY file blocks ALL commits
- Fix ALL gaps before committing

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

**Never use exclusions without explicit user permission.**

Coverage philosophy: Gaps are learning opportunities showing where design can improve. Fix the design, don't silence the messenger.

## Code Style

**Comments:**
- Use `//` style only (never `/* ... */`)
- Comment why, not what
- Use sparingly

**Numeric Types:**
- Always use `<inttypes.h>` for numeric types and format specifiers
- Never use primitive types (`int`, `long`, etc.)
- Use explicit sized types: `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, etc.
- Use `size_t` for sizes and counts
- Use `PRId32`, `PRIu64`, etc. for printf format specifiers
- Use `SCNd32`, `SCNu64`, etc. for scanf format specifiers

Example:
```c
#include <inttypes.h>

int32_t count = 42;
uint64_t size = 1024;
printf("Count: %" PRId32 ", Size: %" PRIu64 "\n", count, size);
```

**Naming Conventions:**

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

See [docs/naming.md](docs/naming.md) for complete conventions and approved abbreviations.

## Test Execution

**Default**: Tests run in parallel (configured via `.envrc`):
- `MAKE_JOBS=32` - up to 32 concurrent tests
- `PARALLEL=1` - all 4 check-dynamic subtargets in parallel

**When you need clear debug output** (serialize execution):
```bash
MAKE_JOBS=1 PARALLEL=0 make check
MAKE_JOBS=1 make check-valgrind
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
