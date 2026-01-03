# Analyze Single Test

**UNATTENDED EXECUTION:** Output only the classification, nothing else.

## Context

The function `{{function}}` is being removed. Test `{{test_name}}` in `{{test_file}}` calls this function.

## Task

Determine if this test is:
- **TESTING**: The test exists to verify `{{function}}` works correctly (test should be deleted)
- **USING**: The test uses `{{function}}` as a helper to test something else (test should be refactored)

## Output Format

Reply with exactly one word: `TESTING` or `USING`
