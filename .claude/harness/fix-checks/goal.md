## Objective

All checks must pass: compile, filesize, check, complexity, sanitize, tsan, valgrind, helgrind, coverage.

## Suggested Progression

This order generally works well. Each check builds on what comes before:

1. **compile** - Does it build? If not, nothing else matters. Don't dig into code logic yet - just make it compile.

2. **filesize** - Are files small enough to reason about? Large files blow up context. Get files manageable before diving deep into them.

3. **check** - Do basic tests pass? Establishes a working baseline. Know the code works before changing it - you need a reference point.

4. **complexity** - Is the code simple enough to modify safely? Complex code is hard to change without introducing bugs. Prefer simplifying before modifying.

5. **sanitize** - Memory safety, undefined behavior. Catches issues the compiler doesn't.

6. **tsan** - Thread safety. Data races, deadlocks. Concurrency bugs are subtle - catch them early.

7. **valgrind** - Memory errors under load. Leaks, invalid access, use-after-free.

8. **helgrind** - Thread errors via valgrind. Another angle on threading issues.

9. **coverage** - Is everything tested? Final completeness check - all code paths exercised.

**The philosophy:** Start mechanical (compiles? fits in context?), establish baseline (tests pass), simplify (reduce complexity), harden (runtime checks), complete (coverage).

Use judgment - if a different order makes faster progress, take it. But understand why this order exists.

## Regression

Changes can break earlier checks. A tsan fix might add code that breaks filesize. A coverage fix might break a unit test.

After making changes, re-verify earlier checks still pass. If something regresses, address it before continuing.

## Hints

- For talloc/ownership issues: `/load memory`
- For Result type patterns: `/load errors`
- For test structure: `/load tdd`
- For PostgreSQL/fixtures: `/load database`
- For naming conventions: `/load style`

## Acceptance

DONE when ALL checks return `{"ok": true}`:
- `.claude/harness/compile/run`
- `.claude/harness/filesize/run`
- `.claude/harness/check/run`
- `.claude/harness/complexity/run`
- `.claude/harness/sanitize/run`
- `.claude/harness/tsan/run`
- `.claude/harness/valgrind/run`
- `.claude/harness/helgrind/run`
- `.claude/harness/coverage/run`
