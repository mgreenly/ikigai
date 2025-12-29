# Fix Helgrind Thread Error

You have ONE job: fix the thread synchronization error described below. Do not refactor unrelated code.

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

1. Read the file and understand the thread error
2. Identify the synchronization issue
3. Add proper locking or synchronization to fix it

## Common Helgrind Errors

- **Possible data race**: Unsynchronized access to shared data
- **Lock order violation**: Potential deadlock from inconsistent lock ordering
- **Locks held by more than one thread**: Lock not properly protecting data

## Synchronization Fixes

- **Add mutex protection** around shared data access
- **Use atomic operations** for simple flags/counters
- **Fix lock ordering** - always acquire locks in consistent order
- **Use condition variables** for thread coordination
- **Make data thread-local** if it shouldn't be shared

## Constraints

- Do NOT add excessive locking (can cause deadlocks)
- Do NOT change the threading model unless necessary
- Do NOT refactor unrelated code
- Keep changes minimal and focused

## Validation

Before reporting done, run:
1. `make check` - ensure tests still pass
2. `make check-helgrind` - ensure thread error is fixed

## When Done

Report what synchronization you added and why. Be brief.
