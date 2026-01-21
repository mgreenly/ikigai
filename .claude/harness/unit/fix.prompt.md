# Fix Unit Test Failure

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the failing unit test. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load style
- /load tdd

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this test. Learn from their attempts:

{{history}}
{{/if}}

## The Failure

**Test File:** {{file}}
**Test Function:** {{function}}
**Line:** {{line}}
**Message:** {{message}}

## Instructions

1. Read the test file to understand what the test is checking
2. Identify why the test is failing
3. Fix the implementation or test with minimal changes
4. Verify the fix by running in order:
   - `.claude/scripts/check-compile`
   - `.claude/scripts/check-link`
   - `.claude/scripts/check-unit`

## Constraints

- Do NOT change test behavior unless the test itself is wrong
- Do NOT refactor beyond fixing the failure
- Keep changes minimal and focused

## When Done

1. Append a brief summary to `.claude/harness/unit/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed. Be brief.
