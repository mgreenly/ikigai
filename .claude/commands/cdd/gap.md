---
description: Find the most critical alignment gap in release plan.
---

**Requires:** `$CDD_DIR` environment variable must be set to the workspace directory.

**Token budget:** Do not exceed 140k tokens. When approaching limit, finish verifying the current item and stop.

## CRITICAL: This Command Verifies PLAN Only

**What this command does:**
- Verifies `$CDD_DIR/plan/*.md` documents
- Checks README → user-stories → plan alignment
- Verifies plan follows project conventions
- Identifies missing specifications in the plan

## Process

1. Read `$CDD_DIR/verified.md` to see what has already been verified
2. Check plan alignment and conventions (see checklist below)
3. When an item is verified, append it to `$CDD_DIR/verified.md`
4. Work incrementally - verify items one at a time, not all at once
5. Report gaps found and suggest fixes

## Alignment Checks

Check top-down alignment through the artifact pyramid:

```
README.md (authoritative - what we're building)
    ↓
user-stories/ (how users will interact)
    ↓
plan/ (implementation decisions and coordination)
```

**Verify:**
- Plan implements what README promises
- Plan supports behaviors demonstrated in user-stories
- Plan decisions are documented (not implicit)
- Plan coordinates shared symbols between implementation units

## Plan Convention Checklist

Verify plan documents follow project conventions:

1. **Naming conventions** - Do all new symbols (functions, types, variables) follow `ik_MODULE_THING` pattern?
2. **Return value conventions** - Do functions return `res_t` with OK()/ERR() patterns?
3. **Memory management** - Are talloc ownership rules specified? Error context lifetime correct?
4. **Integration points** - Are call sites and signature changes specified completely?
5. **Research references** - Does plan reference `$CDD_DIR/research/` documents appropriately?
6. **Build changes** - Are Makefile updates identified (new targets, dependencies, flags)?
7. **Database migrations** - If schema changes, are migrations specified?
8. **Test strategy** - Does plan specify what should be tested and how?
9. **No function bodies** - Plan should have signatures and contracts, not implementation code
10. **Library constraints** - Are allowed/forbidden dependencies specified?

This list is not exhaustive. Identify other gaps relevant to the specific plan.

## What Makes a Good Plan?

The plan is the **coordination layer** - it specifies everything that independent tasks must agree on:

**Plan SHOULD include:**
- Public function signatures (names, parameters, return types)
- Struct definitions (member names and types)
- Enum values and meanings
- Integration points and call chains
- Error handling approach
- Memory ownership rules
- Behavioral contracts ("when X happens, do Y")

**Plan should NOT include:**
- Function bodies or implementation code
- Detailed algorithms or control flow
- Working test code
- Task sequencing or dependencies (that's for order.json)

**Key insight:** If two tasks need to agree on something, the plan must decide it.
