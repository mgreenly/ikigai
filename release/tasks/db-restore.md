# Task: Restore Thinking Blocks from Database

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** db-persistence.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For error handling patterns
- `/load style` - For code style conventions

**Source:**
- `src/message.c` - Message from DB (lines 77-130)
- `src/msg.h` - Database message struct
- `src/providers/request.h` - Content block builders

**Plan:**
- `release/plan/thinking-signatures.md` - Section 10 (Message Restoration)

## Libraries

- `yyjson` - Already used for JSON parsing

## Preconditions

- [ ] Git workspace is clean
- [ ] `db-persistence.md` task completed (thinking stored in DB)

## Objective

When loading tool_call messages from database, reconstruct thinking blocks from the data_json field.

## Interface

### Update `ik_message_from_db_msg`

**File:** `src/message.c`

The existing function handles tool_call messages. Update to parse thinking from data_json:

```c
// In the tool_call handling section:
if (strcmp(db_msg->kind, "tool_call") == 0 && db_msg->data_json != NULL) {
    yyjson_doc *doc = yyjson_read(db_msg->data_json, strlen(db_msg->data_json), 0);
    if (doc != NULL) {
        yyjson_val *root = yyjson_doc_get_root(doc);

        // Count blocks needed
        size_t block_count = 1;  // tool_call
        yyjson_val *thinking_obj = yyjson_obj_get(root, "thinking");
        yyjson_val *redacted_obj = yyjson_obj_get(root, "redacted_thinking");
        if (thinking_obj != NULL) block_count++;
        if (redacted_obj != NULL) block_count++;

        // Create message with appropriate block count
        ik_message_t *msg = talloc_zero(ctx, ik_message_t);
        if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        msg->role = IK_ROLE_ASSISTANT;
        msg->content_blocks = talloc_array(msg, ik_content_block_t, (unsigned int)block_count);
        if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        msg->content_count = block_count;

        size_t idx = 0;

        // Restore thinking block (if present)
        if (thinking_obj != NULL && yyjson_is_obj(thinking_obj)) {
            yyjson_val *text_val = yyjson_obj_get(thinking_obj, "text");
            yyjson_val *sig_val = yyjson_obj_get(thinking_obj, "signature");

            msg->content_blocks[idx].type = IK_CONTENT_THINKING;
            const char *text = yyjson_get_str(text_val);
            msg->content_blocks[idx].data.thinking.text = talloc_strdup(msg, text ? text : "");
            if (!msg->content_blocks[idx].data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            const char *sig = yyjson_get_str(sig_val);
            if (sig != NULL) {
                msg->content_blocks[idx].data.thinking.signature = talloc_strdup(msg, sig);
                if (!msg->content_blocks[idx].data.thinking.signature) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            } else {
                msg->content_blocks[idx].data.thinking.signature = NULL;
            }
            idx++;
        }

        // Restore redacted thinking (if present)
        if (redacted_obj != NULL && yyjson_is_obj(redacted_obj)) {
            yyjson_val *data_val = yyjson_obj_get(redacted_obj, "data");

            msg->content_blocks[idx].type = IK_CONTENT_REDACTED_THINKING;
            const char *data = yyjson_get_str(data_val);
            msg->content_blocks[idx].data.redacted_thinking.data = talloc_strdup(msg, data ? data : "");
            if (!msg->content_blocks[idx].data.redacted_thinking.data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            idx++;
        }

        // Restore tool_call (with null checks for corrupt JSON)
        yyjson_val *id_val = yyjson_obj_get(root, "tool_call_id");
        yyjson_val *name_val = yyjson_obj_get(root, "tool_name");
        yyjson_val *args_val = yyjson_obj_get(root, "tool_args");

        const char *id_str = yyjson_get_str(id_val);
        const char *name_str = yyjson_get_str(name_val);
        const char *args_str = yyjson_get_str(args_val);

        // If any required field is missing, fall back to legacy parsing
        if (id_str == NULL || name_str == NULL || args_str == NULL) {
            yyjson_doc_free(doc);
            // Fall through to legacy content-based parsing below
        } else {
            msg->content_blocks[idx].type = IK_CONTENT_TOOL_CALL;
            msg->content_blocks[idx].data.tool_call.id = talloc_strdup(msg, id_str);
            if (!msg->content_blocks[idx].data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            msg->content_blocks[idx].data.tool_call.name = talloc_strdup(msg, name_str);
            if (!msg->content_blocks[idx].data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            msg->content_blocks[idx].data.tool_call.arguments = talloc_strdup(msg, args_str);
            if (!msg->content_blocks[idx].data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_doc_free(doc);
            *out = msg;
            return OK(msg);
        }
    }
}
```

### Backward Compatibility

Handle messages stored before thinking support:

```c
// If data_json is "{}" or doesn't have new fields, fall back to content parsing
if (doc == NULL || yyjson_obj_get(root, "tool_call_id") == NULL) {
    // Existing code path for old-format messages
    *out = ik_message_create_tool_call(ctx, /* parse from content */);
}
```

## Behaviors

- Messages with thinking in data_json get multi-block content
- Old messages without thinking still work (single tool_call block)
- Missing signature is handled (set to NULL)
- Corrupt JSON falls back to content-only parsing

## Test Scenarios

Add tests in `tests/unit/message_test.c`:

1. `test_from_db_tool_call_with_thinking` - Verify thinking block restored
2. `test_from_db_tool_call_with_signature` - Verify signature restored
3. `test_from_db_tool_call_with_redacted` - Verify redacted_thinking restored
4. `test_from_db_tool_call_no_thinking` - Verify backward compatibility
5. `test_from_db_tool_call_empty_json` - Verify "{}" handled

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(db-restore.md): success - restore thinking blocks from database

ik_message_from_db_msg now reconstructs thinking blocks from
data_json when loading tool_call messages. Backward compatible
with old messages.
EOF
)"
```

Report status: `/task-done db-restore.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
