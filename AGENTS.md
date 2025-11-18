# Agent Notes

docs/README.md provides an overview of the project.

## Core Principles

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

**Coverage Requirements**

**CRITICAL: 100% coverage of Lines, Functions, and Branches for the ENTIRE codebase.**

The requirement is ABSOLUTE:
- 100% of ALL code, not just new code
- A single uncovered line or branch in ANY file blocks ALL commits
- Fix ALL gaps before committing

**Never use coverage exclusions without explicit user permission.**

Coverage philosophy: Gaps are learning opportunities showing where design can improve. Fix the design, don't silence the messenger.

**Coverage Exclusion Policy**

`LCOV_EXCL_BR_LINE` is ONLY permitted for:
1. Asserts on invariants (compiled out in production builds)
2. PANIC lines asserting invariants (unconditionally terminate)

NEVER use `LCOV_EXCL_BR_LINE` for:
- Defensive programming
- Library error returns
- Any code path reachable at runtime

All runtime-reachable branches require actual test coverage. If a library can return an error, write tests that trigger that error. Coverage exclusions are not a substitute for testing.

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
