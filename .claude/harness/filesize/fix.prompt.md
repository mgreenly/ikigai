# Split Oversized File

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: split the oversized file into smaller, well-organized modules. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load makefile
- /load memory
- /load naming
- /load source-code
- /load style

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Problem

**File:** {{file}}
**Current size:** {{bytes}} bytes
**Limit:** {{limit}} bytes

## Instructions

1. Read the oversized file to understand its structure
2. Identify logical groupings of functions that can be extracted
3. Create new file(s) with appropriate names for extracted code
4. Update the original file to include the new header(s)
5. Update any other files that need to include the new headers
6. Ensure all #include statements are correct

## Splitting Guidelines

- Group related functions together (by feature, by layer, by type)
- Name new files descriptively (e.g., `foo_utils.c`, `foo_io.c`, `foo_parse.c`)
- Keep header files alongside their .c files
- Move static helpers with the functions that use them
- Update the corresponding header file if public APIs are moved

## Constraints

- Do NOT change function signatures or behavior
- Do NOT rename functions (unless moving to avoid conflicts)
- Do NOT refactor code beyond what's needed for the split
- Keep changes focused on reducing file size

## Makefile Updates

If splitting test files and creating `*_helpers.c`:
1. Add `*_HELPERS_OBJ` variable
2. Add compile rule for the helper `.o`
3. Add helper to link rules for tests that use it

Follow existing patterns in Makefile (search for `_HELPERS_OBJ`).

## Validation

Before reporting done, run:
1. `make clean && make check-compile` - ensure compilation succeeds
2. `make filesize` - ensure the file is now under the limit

## When Done

1. Append a brief summary to `.claude/harness/filesize/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what files you created and how you organized the split. Be brief.
