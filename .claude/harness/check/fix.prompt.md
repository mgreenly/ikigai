# Fix Test Failure

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the failing test described below. Do not fix other tests. Do not refactor unrelated code. Fix this ONE issue and stop.

## Load Required Skills

Before starting, load these skills for context:
- /load ctags
- /load errors
- /load makefile
- /load memory
- /load mocking
- /load naming
- /load testability

## The Failure

**File:** {{file}}
**Line:** {{line}}
**Error:** {{message}}

## Make Output (tail)

```
{{make_output}}
```

## Instructions

1. Read the failing test file to understand what it's testing
2. Read the source file(s) being tested
3. Identify the root cause of the failure
4. Fix the issue - prefer fixing the source code over modifying test expectations
5. Only modify test expectations if they are clearly incorrect
6. Verify your fix by running: `make check TEST={{file}}`
7. If the test still fails, continue fixing until it passes

## Constraints

- Do NOT run `make check` without TEST= - always use `make check TEST={{file}}`
- Do NOT fix other failing tests you might notice
- Do NOT refactor code beyond what's needed for this fix
- Do NOT add new features or improvements
- Keep changes minimal and focused

## When Done

Report what you changed and why. Be brief.
