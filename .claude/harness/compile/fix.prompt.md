# Fix Compile Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the compile error. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load makefile
- /load memory
- /load style

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Error

**File:** {{file}}
**Line:** {{line}}
**Error:** {{message}}

## Investigating the Error

To see the full compiler output for this file, run:
```bash
.claude/scripts/check-compile --file={{file}}
```

This will return JSON with detailed error information.

## Instructions

1. Run `.claude/scripts/check-compile --file={{file}}` to see full compiler errors
2. Read the file at the error location
3. Understand the error (missing include, undeclared symbol, type mismatch, etc.)
4. Fix the error with minimal changes
5. If the fix requires adding an include, check that the header exists
6. If the fix requires a declaration, find where the symbol is defined

## Common Fixes

- **undeclared identifier**: Add missing #include or forward declaration
- **implicit declaration of function**: Add missing #include for the header declaring it
- **incompatible types**: Check function signature matches declaration
- **expected ';'**: Syntax error, often a missing semicolon or brace
- **missing prototypes**: Add function declarations to header file

## Constraints

- Do NOT change function behavior
- Do NOT refactor beyond fixing the error
- Do NOT add code that wasn't needed before
- Keep changes minimal and focused

## Validation

Run `.claude/scripts/check-compile` to verify the error is fixed.

## When Done

1. Append a brief summary to `.claude/harness/compile/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed. Be brief.
