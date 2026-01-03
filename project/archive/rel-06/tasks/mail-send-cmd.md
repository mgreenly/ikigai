# Task: /send Command

## Target
User Story: 15-send-mail.md

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
- src/commands.c (READ - command parsing patterns, MODIFY - add cmd_send)

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c (READ - test structure patterns)

## Files to Create
- tests/unit/commands/cmd_send_test.c (CREATE - new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task mail-db-operations.md must be complete (provides ik_db_mail_insert, ik_mail_msg_create)

## Task
Implement the `/send` command for sending mail to another agent.

**Command:**
```
/send <uuid> "message body"
```

**Recipient Validation (Q11):**
- Check recipient agent exists AND has status='running'
- If recipient is dead, fail with error "Recipient agent is dead"
- Self-mail is allowed (Q12)

**Flow:**
1. Parse recipient UUID and message body
2. Validate recipient exists
3. Validate recipient is running (not dead)
4. Validate message body non-empty
5. Create mail message
6. Insert into database
7. Display confirmation

## TDD Cycle

### Red
1. Create `tests/unit/commands/cmd_send_test.c`:
   - Test /send creates mail record
   - Test mail has correct from/to UUIDs
   - Test mail body stored correctly
   - Test non-existent recipient shows error
   - Test dead recipient shows "Recipient agent is dead" error
   - Test dead recipient does NOT create mail record
   - Test self-mail is allowed (sender == recipient)
   - Test empty body shows error
   - Test confirmation displayed

2. Run `make check` - expect test failures

### Green
1. Implement `cmd_send()` in `src/commands.c`:
   ```c
   res_t cmd_send(ik_repl_ctx_t *repl, const char *args)
   {
       // Parse: <uuid> "message"
       char uuid[32];
       char body[4096];
       if (parse_send_args(args, uuid, sizeof(uuid),
                           body, sizeof(body)) != 0) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Usage: /send <uuid> \"message\"");
           return OK(NULL);
       }

       // Validate recipient exists
       ik_agent_ctx_t *recipient = ik_repl_find_agent(repl, uuid);
       if (recipient == NULL) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Agent not found");
           return OK(NULL);
       }

       // Validate recipient is running (Q11)
       // Note: in-memory agents are always running (dead agents removed from array)
       // But also check registry for consistency
       agent_status_t status;
       CHECK(ik_db_agent_get_status(repl->shared->db, recipient->uuid, &status));
       if (status != AGENT_STATUS_RUNNING) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Recipient agent is dead");
           return OK(NULL);
       }

       // Validate body
       if (body[0] == '\0') {
           ik_scrollback_append_error(repl->current->scrollback,
               "Message body cannot be empty");
           return OK(NULL);
       }

       // Create and insert
       ik_mail_msg_t *msg = ik_mail_msg_create(repl,
           repl->current->uuid, recipient->uuid, body);
       CHECK(ik_db_mail_insert(repl->shared->db,
           repl->shared->session_id, msg));

       // Confirm
       char confirm[64];
       snprintf(confirm, sizeof(confirm), "Mail sent to %.22s",
           recipient->uuid);
       ik_scrollback_append_system(repl->current->scrollback, confirm);

       return OK(NULL);
   }
   ```

2. Register command

3. Run `make check` - expect pass

### Refactor
1. Extract argument parsing helper
2. Run `make lint` - verify clean
3. Commit changes with descriptive message

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/send` command works correctly
- Mail persisted to database
- Error handling for invalid input
- Working tree is clean (all changes committed)
