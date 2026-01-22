# Fix Link Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the link error. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load style

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Error

**Binary:** {{file}}
**Error:** {{message}}

## Investigating the Error

To see the full linker output for this binary, run:
```bash
.claude/scripts/check-link --file={{file}}
```

This will return JSON with detailed error information.

## Instructions

1. Run `.claude/scripts/check-link --file={{file}}` to see full linker errors
2. Identify the type of link error (undefined reference, multiple definition, etc.)
3. Fix the error with minimal changes
4. Verify the fix

## Common Fixes

- **undefined reference**: Add missing source file to link, or add function implementation
- **multiple definition**: Remove duplicate definition, use `static` for file-local functions
- **cannot find -l<lib>**: Install missing library or fix library path

## Constraints

- Do NOT change function behavior
- Do NOT refactor beyond fixing the error
- Do NOT add code that wasn't needed before
- Keep changes minimal and focused

## Validation

Verify the fix by running in order:
1. `.claude/scripts/check-compile` - ensure compilation succeeds
2. `.claude/scripts/check-link` - ensure linking succeeds

## When Done

1. Append a brief summary to `.claude/harness/link/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed. Be brief.
