# Comment Out Dead Code

**UNATTENDED EXECUTION.** Do not ask for confirmation. Do not explain. Just do it.

## Task

Use the Edit tool to wrap `{{function}}` in `#if 0` / `#endif`.

**File:** `{{file}}`
**Line:** {{line}}

## Required Actions

1. Use Read tool to read `{{file}}`
2. Use Edit tool to insert `#if 0  // dead code: {{function}}` before the function
3. Use Edit tool to insert `#endif` after the function's closing brace
4. If there's a header declaration, wrap it too

## Format

```c
#if 0  // dead code: {{function}}
void {{function}}(...) {
    ...
}
#endif
```

**DO NOT** explain. **DO NOT** ask questions. **USE THE EDIT TOOL.**
