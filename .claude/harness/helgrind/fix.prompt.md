# Fix Helgrind Thread Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the thread synchronization error described below. Do not refactor unrelated code.

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

1. Read the file and understand the thread error
2. Identify the synchronization issue
3. Add proper locking or synchronization to fix it

## Common Helgrind Errors

- **Possible data race**: Unsynchronized access to shared data
- **Lock order violation**: Potential deadlock from inconsistent lock ordering
- **Locks held by more than one thread**: Lock not properly protecting data

## Synchronization Fixes

- **Add mutex protection** around shared data access
- **Use atomic operations** for simple flags/counters
- **Fix lock ordering** - always acquire locks in consistent order
- **Use condition variables** for thread coordination
- **Make data thread-local** if it shouldn't be shared

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
4. `.claude/scripts/check-helgrind`

## When Done

1. Append a brief summary to `.claude/harness/helgrind/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what synchronization you added and why. Be brief.
