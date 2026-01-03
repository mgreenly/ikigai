# Reduce Function Complexity

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. Diagnose and fix the issue autonomously. Do not stop to ask for confirmation or clarification.

You have ONE job: reduce the complexity of the function described below. Do not refactor unrelated code.

## Load Required Skills

Before starting, load these skills for context:
- /load errors
- /load makefile
- /load memory
- /load naming
- /load style
- /load testability

## The Problem

**File:** {{file}}
**Line:** {{line}}
**Function:** {{function}}
**Issue:** {{issue_type}} score {{score}}

## Make Output (tail)

```
{{make_output}}
```

## Instructions

1. Read the file and understand the complex function
2. Identify sources of complexity (deep nesting, many branches, long chains)
3. Refactor to reduce complexity while preserving exact behavior

## Refactoring Strategies

- **Extract helper functions** for logical sub-operations
- **Early returns** to reduce nesting depth
- **Guard clauses** at function start for error conditions
- **Lookup tables** instead of long switch/if-else chains
- **Split into phases** (validate, process, output)

## Constraints

- Do NOT change the function's public interface
- Do NOT change observable behavior
- Do NOT refactor other functions unless extracting helpers
- Keep changes focused on the identified function

## Validation

Before reporting done, run:
1. `make check` - ensure tests still pass
2. `make complexity` - ensure complexity is now acceptable

## When Done

Report what refactoring you applied. Be brief.
