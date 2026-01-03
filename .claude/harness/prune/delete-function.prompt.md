# Delete Dead Function

**UNATTENDED EXECUTION.** Do not ask for confirmation. Do not explain. Just do it.

## Task

Delete the function `{{function}}` from the codebase entirely.

**Source file:** `{{file}}`

## Required Actions

1. Read `{{file}}`
2. Delete the entire function definition (signature through closing brace)
3. Find and read the corresponding header file
4. Delete the function declaration from the header
5. Delete any associated documentation comments

## Rules

- Use the Edit tool for all changes
- Delete completely, do not comment out
- Do NOT modify any other functions
- Do NOT modify test files
