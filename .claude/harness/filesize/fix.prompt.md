# Split Oversized File

You have ONE job: split the oversized file into smaller, well-organized modules. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load makefile
- /load memory
- /load naming
- /load source-code
- /load style

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

## Validation

Before reporting done, run:
1. `make check` - ensure tests still pass
2. `make filesize` - ensure the file is now under the limit

## When Done

Report what files you created and how you organized the split. Be brief.
