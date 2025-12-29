# Fix Sanitizer Error

You have ONE job: fix the memory or undefined behavior error described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load style

## The Error

**File:** {{file}}
**Line:** {{line}}
**Error Type:** {{error_type}}
**Message:** {{message}}

**Stack Trace:**
```
{{stack}}
```

## Make Output (tail)

```
{{make_output}}
```

## Instructions

1. Read the file and understand the error location
2. Analyze the root cause (buffer overflow, use-after-free, null deref, etc.)
3. Fix the underlying bug - don't just suppress the error

## Common Sanitizer Errors

- **heap-buffer-overflow**: Reading/writing past allocated memory
- **stack-buffer-overflow**: Array index out of bounds on stack
- **use-after-free**: Accessing memory after it was freed
- **double-free**: Freeing the same memory twice
- **null-dereference**: Dereferencing a null pointer
- **signed integer overflow**: Arithmetic overflow in signed types
- **shift-exponent**: Shift by negative or too-large amount
- **memory leak**: Allocated memory not freed

## Constraints

- Do NOT change function signatures unless necessary
- Do NOT add defensive NULL checks everywhere - fix the root cause
- Do NOT refactor unrelated code
- Keep changes minimal and focused

## Validation

Before reporting done, run:
1. `make check` - ensure tests still pass
2. `make check-sanitize` - ensure sanitizer errors are fixed

## When Done

Report what you fixed and why it was causing the error. Be brief.
