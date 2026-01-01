# Justify Test Helper Creation

**UNATTENDED EXECUTION:** Analyze and provide justification or rejection.

## Context

The function `{{function}}` is dead code - it is not reachable from main() in production. Tests in `{{test_file}}` call this function.

We have already tried:
1. Deleting the tests that use this function - FAILED
2. Refactoring tests to not call this function - FAILED

## Question

Should this function be preserved as a test helper?

## Analysis Required

To justify creating a helper, you must find **affirmative proof** that:
1. The function provides genuine test infrastructure value (mocking, setup, utilities)
2. The function exists for test-only purposes by design
3. There is a logical reason this functionality should exist even though production doesn't use it

## Common Cases (NO helper needed)

- Tests are testing the dead function itself (e.g., `test_{{function}}_*`)
- Tests verify the function's return values or behavior
- The function was production code that became dead

These cases → delete the tests, do NOT create a helper

## Rare Cases (helper justified)

- Function creates test fixtures or mock objects
- Function manipulates internal state for testing
- Function is explicitly test infrastructure (naming, location, purpose)

## Output Format

Respond with EXACTLY one of these formats:

If NO justification found:
```
DECISION: REJECT
REASON: [one sentence explanation why tests should be deleted]
```

If justification found:
```
DECISION: APPROVE
REASON: [one sentence explanation of test infrastructure value]
```

## Rules

- Default to REJECT unless you find clear proof
- The burden of proof is on preservation, not deletion
- Do NOT create or modify any files
- Just analyze and output the decision
