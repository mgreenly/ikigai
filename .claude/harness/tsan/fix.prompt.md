# Fix ThreadSanitizer Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the data race or thread safety error described below. Do not refactor unrelated code.

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

### ThreadSanitizer Output
```
{{items}}
```

## Instructions

1. Read the file at the error location
2. Identify the shared data being accessed without synchronization
3. Determine the appropriate fix:
   - Add mutex/lock protection
   - Use atomic operations
   - Restructure to avoid shared mutable state
4. Fix the underlying race condition - don't just suppress the warning

## Common ThreadSanitizer Errors

- **data race**: Two threads accessing same memory, at least one writing, without synchronization
- **lock-order-inversion**: Potential deadlock from inconsistent lock ordering
- **thread leak**: Thread created but never joined
- **mutex misuse**: Unlocking mutex not held, destroying locked mutex, etc.

## Constraints

- Do NOT change function signatures unless necessary
- Do NOT add locks everywhere - understand the data flow first
- Do NOT refactor unrelated code
- Keep changes minimal and focused
- Prefer fine-grained locking over coarse-grained when possible

## Validation

Verify the fix by running in order:
1. `.claude/scripts/check-compile`
2. `.claude/scripts/check-link`
3. `.claude/scripts/check-unit`
4. `.claude/scripts/check-integration`
5. `.claude/scripts/check-tsan`

## When Done

1. Append a brief summary to `.claude/harness/tsan/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed and why it was causing the race condition. Be brief.
