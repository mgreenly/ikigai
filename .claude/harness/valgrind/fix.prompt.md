# Fix Valgrind Memory Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the memory error described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load source-code

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Error

**File:** {{file}}
**Line:** {{line}}
**Error Type:** {{error_type}}
**Message:** {{message}}

**Stack Trace:**
```
{{stack}}
```

## Make Output (tail)

```
{{make_output}}
```

## Instructions

1. Read the file and understand the memory error
2. Trace the data flow to find the root cause
3. Fix the underlying bug

## Common Valgrind Errors

- **Invalid read/write**: Accessing freed memory or out of bounds
- **Uninitialised value**: Using memory before initializing it
- **Definitely lost**: Memory allocated but never freed (leak)
- **Indirectly lost**: Memory only reachable through leaked memory
- **Invalid free**: Freeing memory not from malloc, or double-free
- **Mismatched free**: Using wrong deallocator (free vs delete)

## Constraints

- Do NOT add defensive NULL checks everywhere - fix the root cause
- Do NOT suppress the error without fixing it
- Do NOT refactor unrelated code
- Keep changes minimal and focused

## Validation

Verify the fix by running in order:
1. `.claude/scripts/check-compile`
2. `.claude/scripts/check-link`
3. `.claude/scripts/check-unit`
4. `.claude/scripts/check-integration`
5. `.claude/scripts/check-valgrind`

## When Done

1. Append a brief summary to `.claude/harness/valgrind/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed and the root cause. Be brief.
