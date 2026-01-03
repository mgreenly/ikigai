# Task: Mail Database Operations

## Target
User Stories: 15-send-mail.md, 16-check-mail.md, 17-read-mail.md

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
- READ: src/db/message.c (INSERT/SELECT patterns)
- READ: src/mail/msg.h (created in mail-msg-struct.md task)

## Pre-read Tests (patterns)
- READ: tests/unit/db/message_test.c

## Files to CREATE
- src/db/mail.h (new header file)
- src/db/mail.c (new implementation file)
- tests/unit/db/mail_ops_test.c (new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- mail-msg-struct.md task complete
- mail-db-schema.md task complete

## Task
Create database operations for mail: insert, query inbox, mark read.

**API:**
```c
// Insert mail message (sets msg->id on success)
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id,
                        ik_mail_msg_t *msg);

// Get inbox for agent (unread first, then by timestamp desc)
res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *mem_ctx,
                       int64_t session_id, const char *to_uuid,
                       ik_mail_msg_t ***out, size_t *count);

// Mark message as read
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);
```

## TDD Cycle

### Red
1. CREATE: `src/db/mail.h` with declarations

2. CREATE: `tests/unit/db/mail_ops_test.c`:
   - Test insert creates record
   - Test insert sets msg->id
   - Test inbox returns messages for recipient only
   - Test inbox orders unread first
   - Test inbox orders by timestamp desc
   - Test mark_read updates read flag

3. Run `make check` - expect test failures

### Green
1. CREATE: `src/db/mail.c`:
   ```c
   res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id,
                           ik_mail_msg_t *msg)
   {
       const char *query =
           "INSERT INTO mail (session_id, from_uuid, to_uuid, body, timestamp) "
           "VALUES ($1, $2, $3, $4, $5) RETURNING id";

       // Execute and set msg->id from result...
   }

   res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *mem_ctx,
                          int64_t session_id, const char *to_uuid,
                          ik_mail_msg_t ***out, size_t *count)
   {
       const char *query =
           "SELECT id, from_uuid, to_uuid, body, timestamp, read "
           "FROM mail "
           "WHERE session_id = $1 AND to_uuid = $2 "
           "ORDER BY read ASC, timestamp DESC";

       // Execute and build result array...
   }

   res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id)
   {
       const char *query = "UPDATE mail SET read = 1 WHERE id = $1";
       // Execute...
   }
   ```

2. MODIFY: Makefile (add src/db/mail.c and tests/unit/db/mail_ops_test.c to build)

3. Run `make check` - expect pass
4. Commit changes (use haiku sub-agent for git commit)

### Refactor
1. Verify proper memory management for result arrays
2. Run `make lint` - verify clean (use haiku sub-agent for verification)
3. If refactoring needed, commit changes (use haiku sub-agent for git commit)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Insert, query, update operations work
- Results properly allocated
- Working tree is clean (all changes committed)
