# Task: /filter-mail Command

## Target
User Story: rel-06/user-stories/18-filter-mail.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Mailbox section)

## Pre-read Source (patterns - READ)
- src/commands.c (look for existing cmd_check_mail or cmd_read_mail patterns)
- src/db/message.h (database API for mail operations)

## Pre-read Tests (patterns - READ)
- tests/unit/commands/*_test.c (command test patterns)

## Files to CREATE/MODIFY
- CREATE: tests/unit/commands/cmd_filter_mail_test.c
- MODIFY: src/commands.c (add cmd_filter_mail function and registration)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- mail-read-cmd.md task is complete (dependency)

## Task
Implement `/filter-mail` command to filter inbox by sender UUID.

**Command:**
```
/filter-mail --from <uuid>
```

**Behavior:**
- Queries mail table filtering by `to_agent_id = current` AND `from_agent_id = <uuid>`
- Orders by: unread first (read ASC), then timestamp descending
- Displays "(filtered by <uuid>, N messages)" in header
- Shows empty result if no matches

## TDD Cycle

### Red
1. Create `tests/unit/commands/cmd_filter_mail_test.c`:
   - Test filters by sender UUID
   - Test partial UUID matching works
   - Test shows "(filtered by ...)" in header
   - Test no matches shows empty result

2. Run `make check` - expect test failures
3. Commit: "Red: Add failing tests for /filter-mail command"

### Green
1. Implement `cmd_filter_mail()` in `src/commands.c`
2. Register command in dispatch table
3. Run `make check` - expect pass
4. Commit: "Green: Implement /filter-mail command"

### Refactor
1. Run `make lint` - verify clean
2. If changes made, commit: "Refactor: Clean up /filter-mail implementation"

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/filter-mail --from <uuid>` filters inbox by sender
- Working tree is clean (all changes committed)
