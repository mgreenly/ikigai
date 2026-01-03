# Task: /delete-mail Command

## Target
User Story: (derived from Q18 decision)

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
- src/commands.c (READ - command parsing patterns, MODIFY - add cmd_delete_mail)
- src/db/mail.h (CREATE - will need ik_db_mail_delete function)

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c (READ - test structure patterns)

## Files to Create
- tests/unit/commands/cmd_delete_mail_test.c (CREATE - new test file)
- src/db/mail.h (CREATE - new header file with mail database operations)
- src/db/mail.c (CREATE - implementation of mail database operations)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task mail-read-cmd.md must be complete (provides /read-mail command)
- Task mail-db-operations.md must be complete (provides mail database operations foundation)

## Task
Implement `/delete-mail` command to permanently delete a message from the mailbox.

**Command:**
```
/delete-mail <id>
```

**Flow:**
1. Parse mail ID from args
2. Validate mail exists
3. Validate mail belongs to current agent (recipient)
4. Delete from database
5. Display confirmation

**Design notes:**
- Only the recipient can delete mail (sender cannot delete mail they sent)
- Deletion is permanent (no soft-delete)
- Delete by mail record ID (not by sender UUID)

## TDD Cycle

### Red
1. CREATE: `tests/unit/commands/cmd_delete_mail_test.c`:
   - Test /delete-mail removes message from database
   - Test confirmation message displayed
   - Test non-existent ID shows error
   - Test ID from different agent (not recipient) shows error
   - Test deleted mail no longer appears in /check-mail
   - Test deleted mail no longer appears in /read-mail

2. Run `make check` - expect test failures (use haiku sub-agent for verification)

### Green
1. CREATE/MODIFY: `src/db/mail.h` and `src/db/mail.c` - Add `ik_db_mail_delete()` function:
   ```c
   res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id,
                           const char *recipient_uuid);
   ```

2. MODIFY: `src/commands.c` - Implement `cmd_delete_mail()` function:
   ```c
   res_t cmd_delete_mail(ik_repl_ctx_t *repl, const char *args)
   {
       // Parse mail ID
       int64_t mail_id = 0;
       if (args == NULL || sscanf(args, "%" SCNd64, &mail_id) != 1) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Usage: /delete-mail <id>");
           return OK(NULL);
       }

       // Delete (validates ownership internally)
       res_t res = ik_db_mail_delete(repl->shared->db,
           mail_id, repl->current->uuid);
       if (is_err(&res)) {
           if (res.err->code == ERR_NOT_FOUND) {
               ik_scrollback_append_error(repl->current->scrollback,
                   "Mail not found or not yours");
           } else {
               return res;
           }
           return OK(NULL);
       }

       // Confirm
       ik_scrollback_append_system(repl->current->scrollback,
           "Mail deleted");

       return OK(NULL);
   }
   ```

3. MODIFY: `src/commands.c` - Register command in command dispatch table

4. Run `make check` - expect pass (use haiku sub-agent for verification)

5. Commit changes (use haiku sub-agent for git commit)

### Refactor
1. Run `make lint` - verify clean (use haiku sub-agent for verification)
2. If refactoring needed, commit changes (use haiku sub-agent for git commit)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/delete-mail` removes message from database
- Only recipient can delete their mail
- Error handling for invalid IDs
- Working tree is clean (all changes committed)
