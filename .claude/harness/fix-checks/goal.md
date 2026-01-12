## Objective

All checks pass: compile, filesize, check, complexity, sanitize, tsan, valgrind, helgrind, coverage.

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

## Tools

Each check has a harness that returns JSON:

| Check | Harness |
|-------|---------|
| compile | `.claude/harness/compile/run` |
| filesize | `.claude/harness/filesize/run` |
| check | `.claude/harness/check/run` |
| complexity | `.claude/harness/complexity/run` |
| sanitize | `.claude/harness/sanitize/run` |
| tsan | `.claude/harness/tsan/run` |
| valgrind | `.claude/harness/valgrind/run` |
| helgrind | `.claude/harness/helgrind/run` |
| coverage | `.claude/harness/coverage/run` |

Returns:
- `{"ok": true}` - check passes
- `{"ok": false, "items": [...]}` - failures with details

## Skills

**Load as needed:**

| Skill | When |
|-------|------|
| `/load errors` | Result types, OK()/ERR() patterns |
| `/load memory` | talloc ownership, contexts |
| `/load database` | PostgreSQL, test fixtures |
| `/load style` | Naming, code patterns |
| `/load tdd` | Test structure, Check framework |

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
