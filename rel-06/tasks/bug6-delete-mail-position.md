# Task: Fix /delete-mail to use inbox position instead of database ID

## Target
Bug from rel-06/mailbox.md: `/delete-mail` treats user input as database ID, but users see inbox positions `[1]`, `[2]`, etc.

## Macro Context

**What:** `/delete-mail N` interprets N as database ID instead of inbox position. This causes "Mail not found or not yours" errors when position doesn't match database ID.

**Why this matters:**
- Users see `[1]`, `[2]` positions in inbox but `/delete-mail 1` fails unpredictably
- `/read-mail` works correctly (same UX), creating inconsistent behavior
- Tests pass by coincidence (they use actual DB IDs, not positions)

**How the bug manifests:**
```
/check-mail
# [1] * from wlT4G... "third message"
# [2]   from wlT4G... "second message"

/read-mail 2    # Works - shows second message
/delete-mail 2  # Error: Mail not found or not yours
```

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

## Pre-read Docs
- rel-06/mailbox.md (full bug analysis with fix code)

## Pre-read Source Patterns
- src/commands_mail.c:257-329 (READ - `cmd_read_mail` - CORRECT pattern to copy)
- src/commands_mail.c:331-365 (READ - `cmd_delete_mail` - BROKEN code to fix)
- src/commands_mail.c:151-255 (READ - `cmd_check_mail` - display shows positions)

## Pre-read Test Patterns
- tests/unit/commands/cmd_read_mail_test.c (READ - correct test approach)
- tests/unit/commands/cmd_delete_mail_test.c (READ - tests to update)

## Source Files to MODIFY
- src/commands_mail.c (`cmd_delete_mail` function)
- tests/unit/commands/cmd_delete_mail_test.c (update to use positions)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes

## Task

Fix `cmd_delete_mail` to use inbox position (matching `cmd_read_mail` pattern) instead of raw database ID.

### Implementation Pattern

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

### Test Updates

Update tests from using DB IDs to using positions:

```c
// BEFORE: Uses database ID (passes by coincidence)
snprintf(args, sizeof(args), "%lld", (long long)mail_id);
res = cmd_delete_mail(test_ctx, repl, args);

// AFTER: Uses position (matches user workflow)
res = cmd_delete_mail(test_ctx, repl, "1");  // Delete first message in inbox
```

### New Tests Required
1. Delete position 1 when multiple messages exist
2. Delete position 2 when multiple messages exist
3. Verify correct message deleted (check remaining inbox contents)
4. Position out of range error (position > count)

## TDD Cycle

### Red
1. Add test `delete_by_position_multi_message` that creates 2 messages
2. Test deletes position 2 (should delete older message, not newer)
3. Run `make check` - test fails because current code treats "2" as DB ID

### Green
1. Refactor `cmd_delete_mail` to:
   - Parse position with `strtoll()` and validate
   - Fetch inbox with `ik_db_mail_inbox()`
   - Validate position in range
   - Map `position - 1` to array index
   - Pass `msg->id` to `ik_db_mail_delete()`
2. Run `make check` - new test passes

### Refactor
1. Update existing tests to use positions instead of DB IDs
2. Ensure error messages are user-friendly ("Message 5 not found" vs "Mail not found")
3. Run `make lint` - passes

## Sub-agent Usage
- Use sub-agent to search for any other commands that might have the same ID vs position bug
- Use sub-agent to verify test coverage is complete

## Overcoming Obstacles
- If inbox ordering affects tests, ensure test setup creates messages in predictable order
- If DB IDs happen to match positions in some tests, add explicit multi-message scenarios

## Post-conditions
- `make` compiles successfully
- `make check` passes
- `make lint` passes
- `/delete-mail N` correctly deletes message at position N
- Tests use positions (user-facing API) not database IDs
- Working tree is clean (all changes committed)

## Deviations
Document any deviation from this plan with reasoning.
