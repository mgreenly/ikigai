# Task: Update Static Function Policy in style.md

## Target

Refactoring #5: Reconsider the Static Function Ban (Phase 1: Foundation)

## Pre-read

### Skills

- default
- style
- quality
- scm

### Source Files

- .agents/skills/style.md (current policy to update)
- rel-06/docs/lcov-static-fn-findings.md (investigation findings)

### Reference

- src/commands.c (example: static command handlers)
- src/layer_input.c (example: static callbacks)
- src/history.c (example: static parsing helpers)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `static-fn-lcov-investigation` is complete
- Task `static-fn-lcov-config` is complete
- Findings document exists and is conclusive

## Task

Replace the blanket "Avoid Static Functions" rule with pragmatic guidelines based on investigation findings.

### What

Update `.agents/skills/style.md` to provide nuanced guidance on when static functions are appropriate vs when to inline code.

### How

1. Read the investigation findings from `rel-06/docs/lcov-static-fn-findings.md`

2. Replace the current "Avoid Static Functions" section with a new "Static Functions" section:

   **If investigation found exclusions work:**
   ```markdown
   ### Static Functions

   Use static functions judiciously with these guidelines:

   **When to use static functions:**
   - Complex logic (>5 lines) repeated within a file
   - Named code blocks for readability
   - Callback implementations (layer callbacks, command handlers)
   - Helper functions with clear single responsibility

   **When to inline instead:**
   - Helper is 1-3 lines and only used once
   - Code is trivially simple (single statement)
   - For absolute coverage certainty (rare edge cases)

   **Coverage handling:**
   - Use LCOV_EXCL_BR_LINE on individual unreachable branches
   - For larger unreachable blocks, use LCOV_EXCL_START/STOP
   - Both markers work correctly in static functions (verified in LCOV X.Y)

   **Exception:** MOCKABLE wrapper functions use static by design.
   ```

   **If investigation found exclusions don't work:**
   ```markdown
   ### Static Functions

   **Limitation:** LCOV exclusion markers in static functions are unreliable.

   **Prefer inlining when:**
   - Function contains PANIC/assert calls
   - Function has unreachable error branches
   - Function needs LCOV exclusion markers

   **Static functions are OK for:**
   - Callbacks (no PANIC paths)
   - Pure functions with full test coverage
   - Helper code where all branches are testable

   **Exception:** MOCKABLE wrapper functions use static by design.
   ```

3. Add a note referencing the investigation document

### Why

The current blanket ban:
1. Is not being followed (50+ static functions exist)
2. Forces code duplication (30+ repeated patterns)
3. May be based on outdated LCOV behavior

Pragmatic guidelines allow:
- Code reuse within files (DRY principle)
- Clear documentation of when static is appropriate
- Informed decisions based on actual LCOV behavior

## TDD Cycle

### Red

This is a documentation task. The "Red" phase is:
1. Current style.md has blanket ban
2. 50+ files violate the ban
3. This creates cognitive dissonance for developers

### Green

1. Update style.md with nuanced guidelines
2. Ensure guidelines match actual LCOV behavior
3. Guidelines should explain existing code (commands.c, etc.)

### Verify

- style.md is updated with clear guidelines
- Guidelines are consistent with investigation findings
- `make check` still passes (no code changes)
- `make lint` still passes

## Post-conditions

- style.md has "Static Functions" section with pragmatic guidelines
- Guidelines document when static functions are appropriate
- Coverage handling is explained
- Existing static functions (commands.c, etc.) now align with documented policy

## Notes

- This is a documentation-only change (low risk)
- The new policy should explain why existing code is acceptable
- Priority: Lower (Phase 1 Foundation per refactor.md)
- Depends on: static-fn-lcov-investigation, static-fn-lcov-config
