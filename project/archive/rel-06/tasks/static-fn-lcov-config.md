# Task: Update LCOV Configuration for Static Functions (If Needed)

## Target

Refactoring #5: Reconsider the Static Function Ban (Phase 1: Foundation)

## Pre-read

### Skills

- default
- style
- quality
- scm

### Source Files

- Makefile (LCOV configuration, lines 93-98, 617-662)
- rel-06/docs/lcov-static-fn-findings.md (findings from investigation task)

### Reference

- .agents/skills/style.md (current static function policy)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `static-fn-lcov-investigation` is complete
- Findings document exists at `rel-06/docs/lcov-static-fn-findings.md`

## Task

Based on investigation findings, update Makefile LCOV configuration if needed to properly handle static function exclusions.

### What

Review the investigation findings and apply any necessary LCOV configuration changes to the Makefile.

### How

1. Read the findings from `rel-06/docs/lcov-static-fn-findings.md`

2. If findings indicate exclusions work correctly:
   - No Makefile changes needed
   - Skip to verification step
   - Commit a small documentation update to Makefile comments

3. If findings indicate configuration issues:
   - Update LCOV flags in Makefile (lines 617-662)
   - Common fixes might include:
     - `--rc lcov_excl_*` settings
     - `--ignore-errors` adjustments
     - Branch coverage flag adjustments
   - Test with `make coverage`

4. If findings indicate fundamental LCOV limitation:
   - Document in Makefile comments why static functions need care
   - Consider alternative approaches (e.g., function-level exclusions)
   - Update `LCOV_EXCL_COVERAGE` threshold if needed

### Why

The Makefile LCOV configuration needs to support whatever policy we adopt for static functions. If the investigation found that exclusions work with certain settings, we should ensure those settings are in place.

## TDD Cycle

### Red

This is a configuration task. The "Red" phase is:
1. If investigation found issues: Current `make coverage` may fail with static functions
2. If investigation found no issues: No red phase needed

### Green

1. Apply necessary Makefile changes (if any)
2. Run `make coverage` to verify 100% coverage maintained
3. Verify exclusion markers work as expected

### Verify

- `make coverage` passes
- LCOV exclusion markers work in static functions (as documented)
- Makefile comments explain any special configuration

## Post-conditions

- LCOV configuration supports static function exclusions (if possible)
- Makefile comments document any limitations
- 100% coverage maintained
- Ready for policy update task

## Notes

- This task may be a no-op if investigation finds everything works
- If configuration changes are needed, test thoroughly
- Priority: Lower (Phase 1 Foundation per refactor.md)
- Depends on: static-fn-lcov-investigation
