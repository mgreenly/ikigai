# Remove Dead Code

**UNATTENDED EXECUTION:** This task runs automatically. Do not ask for confirmation.

## Task

Remove `{{function}}` from `{{file}}` (line {{line}}).

## Steps

1. Read the file and locate the function
2. Delete the function and any preceding doc comment
3. If the header declares it, remove the declaration too

## Rules

- ONLY modify `{{file}}` and its corresponding header (if any)
- Do NOT modify test files
- Do NOT modify other source files
- Do NOT remove other functions or includes

The harness verifies build and tests after you finish.
