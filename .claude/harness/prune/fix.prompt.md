# Remove Dead Code

**UNATTENDED EXECUTION:** This task runs automatically. Do not ask for confirmation.

## Task

Remove `{{function}}` from `{{file}}` (line {{line}}).

## Steps

1. Read the file and locate the function
2. Delete the function and any preceding doc comment
3. If the header declares it, remove that too
4. Do NOT remove other functions or includes

## If Tests Fail

If removing this function breaks tests, those tests are also dead code. Remove or fix them.

The harness will verify build and tests after you finish.
