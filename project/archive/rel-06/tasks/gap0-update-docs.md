# Task: Update Documentation

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 13 of the message type unification. The `.agents/skills/mocking.md` file documents the `ik_msg_from_db_()` mock wrapper which no longer exists. We need to remove this reference.

## Pre-read Skills
- .agents/skills/default.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- .agents/skills/mocking.md (READ - find ik_msg_from_db_ reference)

## Source Files to MODIFY
- .agents/skills/mocking.md (MODIFY - remove ik_msg_from_db_ reference)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Prior tasks completed:
  - gap0-remove-obsolete.md (ik_msg_from_db_ no longer exists)

## Task

### Update mocking.md

Find and remove the row in the mock wrapper table that references `ik_msg_from_db_`:

```markdown
<!-- BEFORE - find this row in the table -->
| `ik_msg_from_db_()` | `ik_msg_from_db()` |

<!-- AFTER - remove the entire row -->
```

The table is around line 159 in mocking.md.

## TDD Cycle

### Red
No tests - this is documentation.

### Green
1. Open .agents/skills/mocking.md
2. Find the mock wrapper table
3. Remove the row containing `ik_msg_from_db_`
4. Verify no other references to `ik_msg_from_db` exist in the file

### Refactor
No refactoring needed.

## Sub-agent Usage
- This is a simple documentation change, no sub-agents needed

## Post-conditions
- No references to `ik_msg_from_db_` in .agents/skills/mocking.md
- Working tree is clean (all changes committed)
