# Fix Test Failure

You have ONE job: fix the failing test described below. Do not fix other tests. Do not refactor unrelated code. Fix this ONE issue and stop.

## Load Required Skills

Before starting, load these skills for context:
- /load ctags
- /load errors
- /load log
- /load memory
- /load naming
- /load style

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

## Constraints

- Do NOT fix other failing tests you might notice
- Do NOT refactor code beyond what's needed for this fix
- Do NOT add new features or improvements
- Keep changes minimal and focused

## When Done

Report what you changed and why. Be brief.
