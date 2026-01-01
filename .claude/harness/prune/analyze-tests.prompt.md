# Analyze Tests Using Dead Function

**UNATTENDED EXECUTION:** Analyze and categorize. Do not modify any files.

## Context

The function `{{function}}` is dead code (not reachable from main). The following tests in `{{test_file}}` call this function:
{{tests_to_analyze}}

## Task

For each test, determine: Is this test **TESTING** the function, or just **USING** it?

**TESTING the function:**
- The test's primary purpose is to verify the function's behavior
- The test name references the function (e.g., `test_{{function}}_*`)
- The test asserts on the function's return value or side effects
- Without this function, the test has no purpose

**USING the function:**
- The test's primary purpose is testing something else
- The function is called for setup, state manipulation, or as a utility
- The test would still have value if the function were replaced with equivalent logic

## Steps

1. Read `{{test_file}}`
2. For each listed test, examine:
   - The test name
   - What the test asserts
   - How the function is used
3. Categorize each test

## Output Format

Respond with EXACTLY this format:

```
TESTING:
- test_name_1
- test_name_2

USING:
- test_name_3
- test_name_4
```

If all tests are TESTING, the USING section should be empty (but still present).
If all tests are USING, the TESTING section should be empty (but still present).

## Rules

- Do NOT modify any files
- Only analyze and categorize
- Be conservative: if uncertain, categorize as USING (we can refactor, not delete)
