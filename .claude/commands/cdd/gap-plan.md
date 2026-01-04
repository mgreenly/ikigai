---
description: Find the most critical alignment gap in release plan.
---

**Requires:** `$CDD_DIR` environment variable must be set to the workspace directory.

**Token budget:** Do not exceed 140k tokens. When approaching limit, finish verifying the current item and stop.

## Process

1. Read `$CDD_DIR/verified.md` to see what has already been verified
2. Check alignment and conventions (see checklist below)
3. When an item is verified, append it to `$CDD_DIR/verified.md`
4. Work incrementally - verify items one at a time, not all at once
5. Report gaps found and suggest fixes

## Alignment Checks

Check top-down alignment:
- `$CDD_DIR/README.md` (goals) -> `$CDD_DIR/user-stories/` (user perspective) -> `$CDD_DIR/plan/` (implementation)
- Verify plan implements what README promises and user-stories demonstrate

## Convention Checklist

Consider these (and other relevant gaps you identify):

1. **Naming conventions** - Do all new names (functions, types, variables) follow project naming conventions?
2. **Return value conventions** - Do all new functions follow the Result/OK()/ERR() return value patterns?
3. **Memory management** - Does the plan describe a consistent memory management approach that follows talloc ownership conventions?
4. **Integration** - Does the plan describe how new code integrates with existing code?
5. **Research references** - Does the plan reference the research documents (`$CDD_DIR/research/`) where necessary?
6. **Build changes** - Does the plan identify if Makefile updates are needed (new targets, dependencies, flags)?
7. **Database migrations** - If the change affects persistent state, does the plan describe required DB migrations?

This list is not exhaustive. Identify other gaps relevant to the specific plan.
