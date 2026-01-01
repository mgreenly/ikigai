# Remove Dead Code

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Remove the function autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: remove the dead code function described below and ensure the build and tests pass.

## Load Required Skills

Before starting, load these skills for context:
- /load git

## The Function

**Function:** {{function}}
**File:** {{file}}
**Line:** {{line}}

## Instructions

1. Read the file and locate the function `{{function}}` at line {{line}}
2. Remove the function completely, including:
   - The function body
   - Any preceding doc comment block (comments immediately before the function)
   - The function declaration/signature
3. If tests depend on this dead code function:
   - Remove or refactor the tests that call it
   - Dead code should not have test coverage - the tests are also dead
4. Do NOT remove:
   - Other production functions
   - Unrelated comments
   - Header includes (even if they appear unused)

## Verification

After removing the function:

1. Run `make bin/ikigai`
   - If build FAILS: this is a false positive (function is actually used)
   - Report: `SKIPPED: {{function}} - false positive`

2. Run `make check`
   - If tests PASS: done
   - If tests FAIL: fix/remove the failing tests, then run `make check` again

3. If tests still fail after fixing:
   - Report: `SKIPPED: {{function}} - test fixes failed`

## Constraints

- Do NOT modify production code beyond removing the target function
- Do NOT remove header includes
- Keep production changes minimal: just delete the function and its doc comment
- Tests may be modified/removed as needed to eliminate dead code dependencies

## Response Format

First word must be SUCCESS or SKIPPED:

```
SUCCESS: {{function}} [optional: N tests modified]
SKIPPED: {{function}} - <reason>
```
