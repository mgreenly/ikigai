---
description: Find the most critical gap in release tasks.
---

**Requires:** `$CDD_DIR` environment variable must be set to the workspace directory.

**Token budget:** Do not exceed 140k tokens. When approaching limit, finish verifying the current item and stop.

## Process

1. Read `$CDD_DIR/verified.md` to see what has already been verified
2. Review tasks for gaps that would prevent successful execution (see checklist below)
3. When an item is verified, append it to `$CDD_DIR/verified.md`
4. Work incrementally - verify items one at a time, not all at once
5. Report gaps found and suggest fixes

## Alignment Checks

- Do tasks align with `$CDD_DIR/plan/`?
- Are all plan items covered by tasks?
- Do tasks reference the correct plan sections?

## Task Checklist

Consider these (and other relevant gaps you identify):

1. **Plan coverage** - Is there a task for each item the plan describes?
2. **Task sequencing** - Are task dependencies correct? Can tasks be executed in the specified order?
3. **Task scope** - Are tasks appropriately sized (not too large to execute in one session)?
4. **Acceptance criteria** - Does each task have clear criteria for "done"?
5. **Test tasks** - Are there tasks for writing tests where the plan requires them?
6. **Build tasks** - If the plan identifies Makefile changes, is there a task for it?
7. **Migration tasks** - If the plan identifies DB migrations, is there a task for it?

This list is not exhaustive. Identify other gaps relevant to the specific tasks.
