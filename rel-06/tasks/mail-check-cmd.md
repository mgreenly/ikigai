# Task: /check-mail Command

## Target
User Story: 16-check-mail.md

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
- src/commands.c (READ - command parsing patterns, MODIFY - add cmd_check_mail)

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c (READ - test structure patterns)

## Files to Create
- tests/unit/commands/cmd_check_mail_test.c (CREATE - new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task mail-send-cmd.md must be complete (provides /send command and mail database operations)

## Task
Implement `/check-mail` command to list inbox contents.

**Command:**
```
/check-mail
```

**Output format:**
```
Inbox (2 messages, 1 unread):

  [1] * from abc123... (2 min ago)
      "Preview of message..."

  [2]   from def456... (1 hour ago)
      "Preview of message..."
```

## TDD Cycle

### Red
1. Create `tests/unit/commands/cmd_check_mail_test.c`:
   - Test displays inbox summary
   - Test shows unread marker (*)
   - Test shows message preview (truncated)
   - Test shows relative timestamp
   - Test empty inbox shows "No messages"
   - Test only shows current agent's mail

2. Run `make check` - expect test failures

### Green
1. Implement `cmd_check_mail()` in `src/commands.c`
2. Register command
3. Run `make check` - expect pass

### Refactor
1. Extract timestamp formatting helper
2. Run `make lint` - verify clean
3. Commit changes with descriptive message

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/check-mail` displays inbox
- Unread messages marked
- Working tree is clean (all changes committed)
