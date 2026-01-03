# Task: /read-mail Command

## Target
User Story: 17-read-mail.md

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

## Pre-read Source (patterns)
- src/commands.c (READ - command parsing patterns, MODIFY - add cmd_read_mail)
- src/db/mail.h (READ - ik_db_mail_mark_read function)

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c (READ - test structure patterns)

## Files to Create
- tests/unit/commands/cmd_read_mail_test.c (CREATE - new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task mail-check-cmd.md must be complete (provides /check-mail command and displays mail IDs)
- Task mail-db-operations.md must be complete (provides ik_db_mail_mark_read function)

## Task
Implement `/read-mail` command to read a specific message and mark it as read.

**Command:**
```
/read-mail <id>
```

## TDD Cycle

### Red
1. CREATE: `tests/unit/commands/cmd_read_mail_test.c`:
   - Test displays full message
   - Test marks message as read
   - Test non-existent ID shows error
   - Test ID from different agent shows error

2. Run `make check` - expect test failures (use haiku sub-agent for verification)

### Green
1. MODIFY: `src/commands.c` - implement `cmd_read_mail()` function
2. MODIFY: `src/commands.c` - register command in command dispatch table
3. Run `make check` - expect pass (use haiku sub-agent for verification)
4. Commit changes (use haiku sub-agent for git commit)

### Refactor
1. Run `make lint` - verify clean (use haiku sub-agent for verification)
2. If refactoring needed, commit changes (use haiku sub-agent for git commit)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/read-mail` displays full message
- Message marked as read
- Working tree is clean (all changes committed)
