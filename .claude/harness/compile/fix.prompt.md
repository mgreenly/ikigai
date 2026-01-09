# Fix Compile/Link Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the compile or link error. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load style

## The Error

**File:** {{file}}
**Line:** {{line}}
**Error:** {{message}}

## Full Compiler Output

```
{{output}}
```

## Instructions

1. Read the file at the error location
2. Understand the error (missing include, undeclared symbol, type mismatch, etc.)
3. Fix the error with minimal changes
4. If the fix requires adding an include, check that the header exists
5. If the fix requires a declaration, find where the symbol is defined

## Common Fixes

- **undeclared identifier**: Add missing #include or forward declaration
- **implicit declaration of function**: Add missing #include for the header declaring it
- **undefined reference**: Symbol defined elsewhere - check if file needs to be added to build, or if declaration is missing
- **incompatible types**: Check function signature matches declaration
- **expected ';'**: Syntax error, often a missing semicolon or brace

## Constraints

- Do NOT change function behavior
- Do NOT refactor beyond fixing the error
- Do NOT add code that wasn't needed before
- Keep changes minimal and focused

## Validation

Run `make` to verify the error is fixed. If new errors appear in the same file, fix those too.

## When Done

Report what you fixed. Be brief.
