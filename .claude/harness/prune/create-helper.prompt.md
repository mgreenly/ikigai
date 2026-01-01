# Create Test Helper

**UNATTENDED EXECUTION:** Do not ask questions. Just create the helper.

## Context

The function `{{function}}` was removed from production code (`{{source_file}}`), but tests still need it for setup/state manipulation. Move it to a test helper.

## Task

Create a test helper header that provides `{{function}}` for tests to use.

## Steps

1. Read `{{source_file}}` to find the original function implementation (it may be wrapped in `#if 0`)
2. Create directory `{{helpers_dir}}` if it doesn't exist
3. Create `{{helpers_dir}}/{{function}}.h` with:
   - The function implementation (as a static inline function)
   - Any required includes
   - Header guards

## Output Format

```c
#ifndef TEST_HELPER_{{function}}_H
#define TEST_HELPER_{{function}}_H

// Required includes here

static inline void {{function}}(void)
{
    // Implementation copied from source
}

#endif
```

## Rules

- Use the Edit tool to create the file
- Copy the EXACT implementation from the source
- Make it `static inline` so it can be included in multiple test files
- Include all necessary headers for the function to compile
- Do NOT modify src/ files
