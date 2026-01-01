# Comment Out Dead Code

**UNATTENDED EXECUTION:** This task runs automatically. Do not ask for confirmation.

## Task

Comment out `{{function}}` in `{{file}}` (line {{line}}).

## Steps

1. Read the file and locate the function
2. Wrap the function in `#if 0` ... `#endif` (including any preceding doc comment)
3. If the header declares it, wrap the declaration in `#if 0` ... `#endif` too

## Example

```c
#if 0  // dead code: {{function}}
/**
 * Doc comment
 */
void {{function}}(...) {
    ...
}
#endif
```

## Rules

- ONLY modify `{{file}}` and its corresponding header (if any)
- Do NOT delete the function - only comment it out with #if 0
- Do NOT modify test files
- Do NOT modify other source files

The harness verifies build and tests after you finish.
