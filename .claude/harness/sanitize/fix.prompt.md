# Fix Sanitizer Error

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: fix the memory or undefined behavior error described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load source-code

{{#if history}}
## Previous Attempts

Other agents have already tried to fix this error. Learn from their attempts:

{{history}}
{{/if}}

## The Error

**Error Type:** {{error_type}}
**Summary:** {{message}}

### Error Location (where crash occurred)
**File:** {{file}}
**Line:** {{line}}

{{#if freed_file}}
### Memory Freed At (THE BUG IS USUALLY HERE)
**File:** {{freed_file}}
**Line:** {{freed_line}}
**Function:** {{freed_function}}
{{/if}}

{{#if allocated_file}}
### Memory Allocated At
**File:** {{allocated_file}}
**Line:** {{allocated_line}}
**Function:** {{allocated_function}}
{{/if}}

### Full Stack Trace
```
{{stack}}
```

### Make Output (tail)
```
{{make_output}}
```

## Instructions

{{#if freed_file}}
**For use-after-free/double-free bugs:**
1. Start at the FREED location (`{{freed_file}}:{{freed_line}}`), not the error location
2. Trace what context/owner the memory was allocated on
3. Understand why it was freed before the error location tried to use it
4. Fix the ownership/lifetime issue - usually passing wrong context to allocator
{{else}}
1. Read the file at the error location
2. Analyze the root cause
3. Fix the underlying bug - don't just suppress the error
{{/if}}

## Common Sanitizer Errors

- **heap-buffer-overflow**: Reading/writing past allocated memory
- **stack-buffer-overflow**: Array index out of bounds on stack
- **use-after-free**: Accessing memory after it was freed (check FREED location first!)
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

1. Append a brief summary to `.claude/harness/sanitize/history.md` describing:
   - What you tried
   - Why you thought it would work
   - What happened (success or failure reason)

2. Report what you fixed and why it was causing the error. Be brief.
