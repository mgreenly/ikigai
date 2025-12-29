# Fix Data Race

You have ONE job: fix the data race described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load memory
- /load mocking
- /load source-code

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

1. Read the file and understand the race condition
2. Identify which shared data is being accessed unsafely
3. Add proper synchronization to fix the race

## Common Data Race Fixes

- **Add mutex protection** around shared data access
- **Use atomic operations** for simple counters/flags
- **Fix initialization order** if racing during setup
- **Copy data to local** before processing if read-only
- **Use thread-local storage** if data shouldn't be shared

## Constraints

- Do NOT add excessive locking (can cause deadlocks)
- Do NOT change the threading model unless necessary
- Do NOT refactor unrelated code
- Keep changes minimal and focused

## Validation

Before reporting done, run:
1. `make check` - ensure tests still pass
2. `make check-tsan` - ensure data race is fixed

## When Done

Report what synchronization you added and why. Be brief.
