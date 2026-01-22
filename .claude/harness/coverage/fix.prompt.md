# Add Test Coverage

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: add tests to cover the gaps in the source file described below. Do not refactor unrelated code. Add tests for this ONE file and stop.

## Load Required Skills

Before starting, load these skills for context:
- /load coverage
- /load errors
- /load lcov
- /load memory
- /load mocking
- /load testability

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Coverage Gap

**Source File:** {{source_file}}
**Current Coverage:** Lines: {{lines_pct}}%, Functions: {{funcs_pct}}%, Branches: {{branches_pct}}%

**Gap Details:**
{{gap_details}}

## Instructions

1. Read the source file to understand what code is uncovered
2. Find the existing test file(s) for this source
3. Add test cases that exercise the uncovered lines/branches/functions
4. Prefer testing real behavior over mocking when possible
5. Use LCOV_EXCL markers ONLY for genuinely untestable code (error paths that can't be triggered)

## Constraints

- Do NOT add coverage for other files
- Do NOT add functions or code to src/ files unless they are called by existing production code
- You may freely add tests, test helpers, and test fixtures in tests/
- Do NOT add LCOV_EXCL markers for code that CAN be tested
- Keep test additions focused and minimal

## Validation

Verify the fix by running in order:
1. `make check-compile`
2. `make check-link`
3. `make check-unit`
4. `make check-integration`
5. `make check-coverage`

## When Done

1. Append a brief summary to `.claude/harness/coverage/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what tests you added and the new coverage for this file. Be brief.
