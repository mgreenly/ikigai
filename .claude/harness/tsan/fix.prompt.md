# Fix Data Race

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the data race described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load mocking
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

1. Read the file and understand the race condition
2. Identify which shared data is being accessed unsafely
3. Add proper synchronization to fix the race

## Common Data Race Fixes

- **Add mutex protection** around shared data access
- **Use atomic operations** for simple counters/flags
- **Fix initialization order** if racing during setup
- **Copy data to local** before processing if read-only
- **Use thread-local storage** if data shouldn't be shared

## Constraints

- Do NOT add excessive locking (can cause deadlocks)
- Do NOT change the threading model unless necessary
- Do NOT refactor unrelated code
- Keep changes minimal and focused

## Validation

Verify the fix by running in order:
1. `.claude/scripts/check-compile`
2. `.claude/scripts/check-link`
3. `.claude/scripts/check-unit`
4. `.claude/scripts/check-tsan`

## When Done

1. Append a brief summary to `.claude/harness/tsan/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what synchronization you added and why. Be brief.
