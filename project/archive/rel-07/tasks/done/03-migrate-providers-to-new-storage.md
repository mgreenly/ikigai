# Task: Migrate Provider Request Building to New Storage

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** 02-migrate-repl-to-new-api.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update provider request builder to read from `agent->messages` array instead of old `agent->conversation`. Providers now use the new storage exclusively. Update tool choice enum to provider-agnostic version.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load naming` - Naming conventions
- `/load style` - Code style

**Source Files to Read:**
- `src/providers/request.h` - Request builder API
- `src/providers/request.c` - `ik_request_build_from_conversation()` implementation
- `src/providers/provider.h` - Request/message struct definitions
- `src/providers/anthropic/request_messages.c` - Reference for similar pattern

## Implementation

### 1. Update src/providers/request.c

**Find function:** `ik_request_build_from_conversation()`

**Current pattern** (reads from agent->conversation):
```c
if (agent->conversation != NULL) {
    for (size_t i = 0; i < agent->conversation->message_count; i++) {
        ik_msg_t *msg = agent->conversation->messages[i];
        // ... convert msg to request format ...
    }
}
```

**New pattern** (reads from agent->messages):
```c
/* Iterate message storage and add to request */
if (agent->messages != NULL) {
    for (size_t i = 0; i < agent->message_count; i++) {
        ik_message_t *msg = agent->messages[i];
        if (msg == NULL) continue;

        /* Deep copy message into request */
        res_t res = ik_request_add_message_direct(req, msg);
        if (is_err(&res)) {
            talloc_free(req);
            return res;
        }
    }
}
```

**Key change:** Messages are already in provider format (`ik_message_t*`), no conversion needed!

### 2. Add ik_request_add_message_direct() helper

**Add to src/providers/request.c before ik_request_build_from_conversation():**

```c
/**
 * Deep copy existing message into request message array
 */
static res_t ik_request_add_message_direct(ik_request_t *req, const ik_message_t *msg)
{
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);  // LCOV_EXCL_BR_LINE

    /* Allocate new message in request context */
    ik_message_t *copy = talloc(req, ik_message_t);
    if (copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    copy->role = msg->role;
    copy->content_count = msg->content_count;
    copy->provider_metadata = NULL;  /* Don't copy response metadata into requests */

    /* Allocate content blocks array */
    copy->content_blocks = talloc_array(copy, ik_content_block_t, msg->content_count);
    if (copy->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Deep copy each content block */
    for (size_t i = 0; i < msg->content_count; i++) {
        ik_content_block_t *src = &msg->content_blocks[i];
        ik_content_block_t *dst = &copy->content_blocks[i];
        dst->type = src->type;

        switch (src->type) {
        case IK_CONTENT_TEXT:
            dst->data.text.text = talloc_strdup(copy, src->data.text.text);
            if (dst->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_TOOL_CALL:
            dst->data.tool_call.id = talloc_strdup(copy, src->data.tool_call.id);
            dst->data.tool_call.name = talloc_strdup(copy, src->data.tool_call.name);
            dst->data.tool_call.arguments = talloc_strdup(copy, src->data.tool_call.arguments);
            if (dst->data.tool_call.id == NULL || dst->data.tool_call.name == NULL ||
                dst->data.tool_call.arguments == NULL) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_TOOL_RESULT:
            dst->data.tool_result.tool_call_id = talloc_strdup(copy, src->data.tool_result.tool_call_id);
            dst->data.tool_result.content = talloc_strdup(copy, src->data.tool_result.content);
            dst->data.tool_result.is_error = src->data.tool_result.is_error;
            if (dst->data.tool_result.tool_call_id == NULL || dst->data.tool_result.content == NULL) {
                PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_THINKING:
            dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
            if (dst->data.thinking.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            break;

        default:
            PANIC("Unknown content type: %d", src->type);  // LCOV_EXCL_LINE
        }
    }

    /* Grow request messages array if needed */
    if (req->message_count >= req->message_capacity) {
        size_t new_capacity = req->message_capacity == 0 ? 16 : req->message_capacity * 2;
        req->messages = talloc_realloc(req, req->messages, ik_message_t*, new_capacity);
        if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        req->message_capacity = new_capacity;
    }

    /* Add to request array */
    req->messages[req->message_count++] = copy;

    return OK(copy);
}
```

### 3. Update src/providers/provider.h

**Find and uncomment tool choice enum:**

```c
/**
 * Tool invocation control modes
 */
typedef enum {
    IK_TOOL_AUTO = 0,     /* Model decides when to use tools */
    IK_TOOL_NONE = 1,     /* No tool use allowed */
    IK_TOOL_REQUIRED = 2, /* Must use a tool */
    IK_TOOL_SPECIFIC = 3  /* Must use specific tool */
} ik_tool_choice_t;
```

**Update ik_request struct:**

Change:
```c
int tool_choice_mode;  /* Tool choice mode (temporarily int during coexistence) */
```

To:
```c
ik_tool_choice_t tool_choice_mode;  /* Tool choice mode */
```

### 4. Remove old conversation references

Search for any remaining references:
```bash
grep -rn "agent->conversation" src/providers/ --include="*.c" --include="*.h"
```

Delete or replace ALL occurrences.

## Test Requirements

### Update Existing Tests

**tests/unit/providers/request_test.c:**
- Update to use `agent->messages` instead of `agent->conversation`
- Verify request built from new storage has correct message count
- Verify content blocks transferred correctly

**tests/integration/providers/*/basic_test.c:**
- Verify end-to-end: add messages → build request → send
- Check request contains all messages from `agent->messages`

### Create New Test

**tests/unit/providers/request_direct_test.c:**

Test cases:
- test_add_message_direct_text - Add text message, verify content
- test_add_message_direct_tool_call - Add tool call, verify fields
- test_add_message_direct_tool_result - Add tool result
- test_add_message_direct_deep_copy - Modify original, verify copy unchanged
- test_add_message_direct_multiple_blocks - Message with multiple content blocks

## Build Verification

```bash
make clean
make all
make check
```

**Expected:**
- Build succeeds
- No references to `agent->conversation` in src/providers/
- All provider tests pass
- All three providers (Anthropic, OpenAI, Google) work

## Postconditions

- [ ] `src/providers/request.c` reads from `agent->messages`
- [ ] `ik_request_add_message_direct()` implemented
- [ ] Tool choice enum uncommented in `provider.h`
- [ ] No `agent->conversation` references in src/providers/
- [ ] All provider tests pass
- [ ] Build succeeds
- [ ] `make check` passes

## Success Criteria

After this task:
1. Provider request building uses ONLY new storage
2. Messages transferred correctly without conversion
3. All three providers work with new format
4. Tests verify correct behavior
5. Ready to migrate restore code
