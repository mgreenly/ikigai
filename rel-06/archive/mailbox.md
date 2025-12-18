# Mailbox Bug: Delete Uses Database ID Instead of Inbox Position

## Summary

`/delete-mail` treats user input as database ID, but users see inbox positions `[1]`, `[2]`, etc. This causes "Mail not found or not yours" errors when the position doesn't match the database ID.

## Observed Behavior

### Test 1: Same session (worked by coincidence)

```
/agents                    # Agent: wlT4G-jnTXaaLmMzn8maZg
/send wlT4G "message"      # Mail sent
/check-mail                # [1] * from wlT4G... "This is your first message"
/delete-mail 1             # Mail deleted (worked because DB ID happened to be 1)
/check-mail                # No messages
```

### Test 2: After restart (failed)

```
# Previous session sent "this is your second message"
# Restart bin/ikigai, history replays

/check-mail                # [1] * from wlT4G... "this is your second message"
/read-mail 1               # Works - displays message
/delete-mail 1             # Error: Mail not found or not yours
```

### Test 3: Multiple messages in same session (partial failure)

```
/agents                    # Agent: wlT4G-jnTXaaLmMzn8maZg
/send wlT4G "third message"
/check-mail
# [1] * from wlT4G... "this is your third message"  (newest)
# [2]   from wlT4G... "this is your second message" (older)

/read-mail 2               # Works - shows second message
/read-mail 1               # Works - shows third message
/delete-mail 2             # Error: Mail not found or not yours
/delete-mail 1             # Mail deleted (coincidence - position matched DB ID)
/check-mail                # [1] from wlT4G... "this is your third message"
/delete-mail 1             # Error: Mail not found or not yours
```

## Database Schema

From `migrations/004-mail-table.sql`:

```sql
CREATE TABLE IF NOT EXISTS mail (
    id BIGSERIAL PRIMARY KEY,          -- Auto-increment, never exposed to user
    session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    from_uuid TEXT NOT NULL,            -- Sender agent UUID
    to_uuid TEXT NOT NULL,              -- Recipient agent UUID
    body TEXT NOT NULL,                 -- Message content
    timestamp BIGINT NOT NULL,          -- Unix timestamp (seconds)
    read INTEGER NOT NULL DEFAULT 0     -- 0=unread, 1=read
);

CREATE INDEX IF NOT EXISTS idx_mail_recipient
ON mail(session_id, to_uuid, read);
```

Key points:
- `id` is a database-internal `BIGSERIAL` (auto-incrementing), never resets per session
- `session_id` scopes mail to current session (CASCADE delete on session end)
- Index optimizes inbox queries by recipient and read status

## Message Structure

From `src/mail/msg.h`:

```c
typedef struct ik_mail_msg {
    int64_t id;           // Database ID (set by INSERT RETURNING)
    char *from_uuid;      // Sender agent UUID
    char *to_uuid;        // Recipient agent UUID
    char *body;           // Message content
    int64_t timestamp;    // Unix timestamp (set on creation)
    bool read;            // Read status
} ik_mail_msg_t;
```

Creation via `ik_mail_msg_create()` sets:
- `id = 0` (populated by database on insert)
- `timestamp = time(NULL)` (current time)
- `read = false` (unread)

## Database API

### ik_db_mail_inbox

Fetches inbox for an agent, returns array of messages.

```c
res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *mem_ctx,
                       int64_t session_id, const char *to_uuid,
                       ik_mail_msg_t ***out, size_t *count);
```

**Query ordering:**
```sql
ORDER BY read ASC, timestamp DESC
```

This means:
1. Unread messages first (`read=0` before `read=1`)
2. Within each group, newest first (higher timestamp first)

The array index `i` maps to display position `[i+1]`.

### ik_db_mail_delete

Deletes a message by database ID, validates ownership.

```c
res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id,
                        const char *recipient_uuid);
```

**Query:**
```sql
DELETE FROM mail WHERE id = $1 AND to_uuid = $2
```

Returns `ERR_IO` with "Mail not found or not yours" if 0 rows affected.

### ik_db_mail_mark_read

Marks a message as read by database ID.

```c
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);
```

**Query:**
```sql
UPDATE mail SET read = 1 WHERE id = $1
```

## Command Implementations

### cmd_read_mail (CORRECT)

Location: `src/commands_mail.c:257-329`

```c
// 1. Parse position from args (1-based)
int64_t index = strtoll(args, &endptr, 10);
if (*endptr != '\0' || index < 1) { /* error */ }

// 2. Fetch inbox
res = ik_db_mail_inbox(db, ctx, session_id, uuid, &inbox, &count);

// 3. Validate position in range
if ((size_t)index > count) { /* error: not found */ }

// 4. Map position to message, get database ID
ik_mail_msg_t *msg = inbox[index - 1];

// 5. Use database ID for operation
res = ik_db_mail_mark_read(db, msg->id);
```

### cmd_delete_mail (BROKEN)

Location: `src/commands_mail.c:331-365`

```c
// 1. Parse "mail_id" directly from args - WRONG!
int64_t mail_id = 0;
sscanf(args, "%" SCNd64, &mail_id);

// 2. Pass directly to database - treats user input as database ID!
res = ik_db_mail_delete(db, mail_id, uuid);
```

**Missing steps:**
- Does NOT fetch inbox
- Does NOT validate position in range
- Does NOT map position → database ID

### cmd_check_mail (display logic)

Location: `src/commands_mail.c:151-255`

```c
// Fetch inbox
res = ik_db_mail_inbox(db, ctx, session_id, uuid, &inbox, &count);

// Display each message with 1-based position
for (size_t i = 0; i < count; i++) {
    // Format: "  [1] * from abc123... (2 min ago)"
    char *msg_line = talloc_asprintf(ctx, "  [%zu] %s from %.22s... (%s)",
                                     i + 1,           // Position (1-based)
                                     msg->read ? " " : "*",
                                     msg->from_uuid,
                                     time_str);
}
```

The display shows `[i+1]` which is the **position**, not the database ID.

## Why Tests Pass But Behavior Is Broken

From `tests/unit/commands/cmd_delete_mail_test.c`:

```c
// Test creates message, captures database ID
ik_mail_msg_t *msg = ik_mail_msg_create(...);
res = ik_db_mail_insert(db, session_id, msg);
int64_t mail_id = msg->id;  // Database ID

// Test passes database ID directly to command
char args[32];
snprintf(args, sizeof(args), "%lld", (long long)mail_id);
res = cmd_delete_mail(test_ctx, repl, args);  // Works!
```

The tests pass because they use the **actual database ID**, not the display position.

In production:
- User sees `[1]` (position)
- User types `/delete-mail 1`
- Command interprets `1` as database ID
- Database ID 1 might not exist (or belong to different session)

## Design Intent

1. **Display**: Inbox shows `[1]`, `[2]`, etc. - positions ordered most recent first
2. **All commands**: Accept position number, map internally to database ID
3. **Database ID**: Hidden implementation detail, never exposed to user

## Fix Required

`cmd_delete_mail` should mirror `cmd_read_mail`:

```c
res_t cmd_delete_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    // 1. Parse position from args (1-based user input)
    char *endptr = NULL;
    int64_t position = strtoll(args, &endptr, 10);
    if (*endptr != '\0' || position < 1) {
        // Error: Invalid position
    }

    // 2. Fetch inbox
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res_t res = ik_db_mail_inbox(repl->shared->db_ctx, ctx,
                                  repl->shared->session_id,
                                  repl->current->uuid,
                                  &inbox, &count);
    if (is_err(&res)) return res;

    // 3. Validate position in range (1 to count)
    if ((size_t)position > count) {
        // Error: Message not found
    }

    // 4. Map position to database ID
    ik_mail_msg_t *msg = inbox[position - 1];

    // 5. Delete using database ID
    res = ik_db_mail_delete(repl->shared->db_ctx, msg->id, repl->current->uuid);

    // ... rest of function
}
```

## Test Updates Required

Tests should be updated to use positions instead of database IDs:

```c
// BEFORE: Uses database ID (passes by coincidence)
snprintf(args, sizeof(args), "%lld", (long long)mail_id);
res = cmd_delete_mail(test_ctx, repl, args);

// AFTER: Uses position (matches user workflow)
res = cmd_delete_mail(test_ctx, repl, "1");  // Delete first message in inbox
```

Also need new tests:
- Delete position 1 when multiple messages exist
- Delete position 2 when multiple messages exist
- Verify correct message deleted (by checking remaining inbox contents)
- Position out of range error

## Files Involved

| File | Purpose |
|------|---------|
| `src/commands_mail.c` | Command handlers - `cmd_delete_mail` needs fix |
| `src/db/mail.h` | Database API declarations |
| `src/db/mail.c` | Database operations (no changes needed) |
| `src/mail/msg.h` | Message structure definition |
| `src/mail/msg.c` | Message creation |
| `migrations/004-mail-table.sql` | Database schema |
| `tests/unit/commands/cmd_delete_mail_test.c` | Tests need position-based approach |

## Related Commands

All mail commands that accept message ID:
- `/read-mail <position>` - CORRECT (fetches inbox, maps position → ID)
- `/delete-mail <position>` - BROKEN (passes input directly as database ID)

Commands that don't need position:
- `/check-mail` - Lists all messages
- `/filter-mail --from <uuid>` - Lists filtered messages
- `/send <uuid> "message"` - Creates new message
