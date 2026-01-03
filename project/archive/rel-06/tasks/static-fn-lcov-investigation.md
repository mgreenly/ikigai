# Task: Investigate LCOV Behavior with Static Functions

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
- src/commands.c (existing static functions, lines 37-41, 234-472)
- src/layer_input.c (existing static callbacks)
- src/history.c (existing static helpers)

### Reference

- .agents/skills/style.md (current static function policy)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Task

Investigate whether LCOV exclusion markers work correctly with static functions and document findings.

### What

Create a minimal test case to verify LCOV_EXCL_START/STOP and LCOV_EXCL_BR_LINE behavior inside static functions. Document whether the current claim in style.md is accurate or outdated.

### How

1. Create a temporary test file `tests/unit/lcov/static_exclusion_test.c`:
   - Define a static function with PANIC call inside
   - Add LCOV_EXCL_BR_LINE marker on the PANIC line
   - Define another static function using LCOV_EXCL_START/STOP block
   - Write tests that exercise non-excluded paths
   - Skip testing the PANIC paths (they would abort)

2. Run `make coverage` and examine the coverage report:
   - Check if the LCOV_EXCL_BR_LINE markers are honored
   - Check if LCOV_EXCL_START/STOP blocks work
   - Document exact behavior observed

3. Check LCOV version and configuration:
   - Run `lcov --version` to document version
   - Review Makefile LCOV flags for branch coverage settings
   - Check if `--rc` settings affect static function handling

4. Document findings in a file `rel-06/docs/lcov-static-fn-findings.md`:
   - LCOV version tested
   - Test methodology
   - Results for LCOV_EXCL_BR_LINE in static functions
   - Results for LCOV_EXCL_START/STOP in static functions
   - Recommendation: keep ban, modify ban, or lift ban

### Why

The style.md rule claims "LCOV exclusion markers on PANIC/assert calls inside static functions are not reliably honored." This claim needs verification:

1. It may be outdated (LCOV behavior may have changed)
2. It may be configuration-dependent (fixable via Makefile changes)
3. It may be accurate (documenting the limitation)

Verifying this claim allows making an informed decision about the static function policy.

## TDD Cycle

### Red

This is an investigation task, not a feature. The "Red" phase is:
1. Run current `make coverage` to establish baseline
2. Note that existing code with static functions (commands.c, etc.) achieves 100% coverage
3. Question: How is this working if the rule says it shouldn't?

### Green

1. Create minimal test reproducing the claimed problem
2. Run coverage and observe actual behavior
3. If exclusions work: Document that claim is outdated
4. If exclusions fail: Document exact failure mode

### Verify

- Created investigation test (can be deleted after findings documented)
- `make coverage` passes (100% coverage maintained)
- Findings documented in `rel-06/docs/lcov-static-fn-findings.md`

## Post-conditions

- LCOV behavior with static functions is documented
- Recommendation for policy change (or confirmation of current policy) is written
- Coverage still passes (investigation didn't break anything)
- All test files created for investigation can be kept (if useful) or deleted

## Notes

- This is a low-risk investigation task
- If investigation shows exclusions work fine, proceed to policy update task
- If investigation shows real problems, document workarounds or keep current policy
- Priority: Lower (Phase 1 Foundation per refactor.md)
