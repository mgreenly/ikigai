# Task 0: Migration Foundation - Assessment and Setup

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** None (this is the first task)

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Assess current migration state, create foundation infrastructure for dual-mode message storage, and verify the system still works. This task creates the plumbing WITHOUT populating it - subsequent tasks will wire it up.

## Current State Assessment

Run these commands to document starting state:

```bash
# What exists now?
echo "=== Directory Structure ===" > scratch/task-0-assessment.txt
ls -la src/openai/ >> scratch/task-0-assessment.txt 2>&1
echo "" >> scratch/task-0-assessment.txt
ls -la src/providers/openai/ >> scratch/task-0-assessment.txt 2>&1
echo "" >> scratch/task-0-assessment.txt

# Old storage exists?
echo "=== Agent Struct Fields ===" >> scratch/task-0-assessment.txt
grep -A 3 "Conversation state" src/agent.h >> scratch/task-0-assessment.txt

# New files exist?
echo "=== New Message Files ===" >> scratch/task-0-assessment.txt
ls -la src/message.* >> scratch/task-0-assessment.txt 2>&1

# Tool call bug fixed?
echo "=== Request Builder Status ===" >> scratch/task-0-assessment.txt
grep -A 2 "Handle tool_call messages" src/providers/request.c >> scratch/task-0-assessment.txt

# Tests passing?
echo "=== Test Status ===" >> scratch/task-0-assessment.txt
make check >> scratch/task-0-assessment.txt 2>&1 && echo "PASS" >> scratch/task-0-assessment.txt || echo "FAIL" >> scratch/task-0-assessment.txt
```

**Expected findings:**
- ✅ `src/openai/` exists (old code, will be deleted in Task 7)
- ✅ `src/providers/openai/shim.c` exists (1000 LOC, currently unused)
- ✅ `agent->conversation` field exists (old storage)
- ❌ `src/message.c/h` do NOT exist (we'll create them)
- ❌ `agent->messages` field does NOT exist (we'll add it)
- ✅ Tool call bug FIXED (commit bc1efb1)
- ✅ Tests PASS (all 146 tests)

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load database` - Database schema, ik_msg_t structure, data_json format
- `/load naming` - Naming conventions (ik_ prefix, approved abbreviations)
- `/load style` - Code style (no static functions, include order)
- `/load tdd` - Test-driven development

**Source Files to Read:**
- `src/providers/provider.h` - Study complete struct definitions:
  - `struct ik_message`: role, content_blocks (array pointer), content_count
  - `struct ik_content_block`: type discriminator + union with 4 variants
  - `ik_role_t` enum: IK_ROLE_USER=0, IK_ROLE_ASSISTANT=1, IK_ROLE_TOOL=2
  - `ik_content_type_t` enum: IK_CONTENT_TEXT=0, IK_CONTENT_TOOL_CALL=1, IK_CONTENT_TOOL_RESULT=2, IK_CONTENT_THINKING=3
- `src/providers/request.c` - Study existing `ik_content_block_text()`, `ik_content_block_tool_call()`, `ik_content_block_tool_result()` functions
- `src/agent.h` - Study `ik_agent_ctx_t` struct - locate conversation field
- `src/db/message.h` - Study `ik_msg_t` for database format conversion

**Test Pattern Files:**
- `tests/unit/db/message_test.c` - Pattern for message creation tests
- `tests/unit/agent/fork_test.c` - Pattern for agent operation tests

## Error Handling Policy

**Memory Allocation Failures:**
- All talloc allocations: PANIC with LCOV_EXCL_BR_LINE
- Rationale: OOM is unrecoverable, panic is appropriate

**Validation Failures:**
- Return ERR allocated on parent context (not on object being freed)
- Example: `return ERR(parent_ctx, INVALID_ARG, "message")`

**Assertions:**
- NULL pointer checks: `assert(ptr != NULL)` with LCOV_EXCL_BR_LINE
- Only for programmer errors, never for runtime conditions

## Implementation

### 1. Create src/message.h

Create header file with these public functions:

```c
#ifndef IK_MESSAGE_H
#define IK_MESSAGE_H

#include "error.h"
#include "providers/provider.h"
#include "db/message.h"
#include <talloc.h>

/**
 * Message Creation API
 *
 * This module provides functions for creating provider-agnostic messages
 * from various sources (user input, LLM responses, tool invocations, database).
 *
 * Memory model: All functions allocate on the provided TALLOC_CTX and
 * use talloc hierarchical ownership for automatic cleanup.
 */

/**
 * Create text message
 *
 * Creates a message with a single text content block.
 *
 * @param ctx  Talloc parent context
 * @param role Message role (USER, ASSISTANT, TOOL)
 * @param text Text content (will be copied)
 * @return     Allocated message, or NULL on OOM (PANIC)
 */
ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text);

/**
 * Create tool call message
 *
 * Creates an ASSISTANT message with a single tool_call content block.
 *
 * @param ctx       Talloc parent context
 * @param id        Tool call ID (will be copied)
 * @param name      Function name (will be copied)
 * @param arguments JSON arguments string (will be copied)
 * @return          Allocated message, or NULL on OOM (PANIC)
 */
ik_message_t *ik_message_create_tool_call(TALLOC_CTX *ctx, const char *id,
                                           const char *name, const char *arguments);

/**
 * Create tool result message
 *
 * Creates a TOOL message with a single tool_result content block.
 *
 * @param ctx          Talloc parent context
 * @param tool_call_id Tool call ID this result is for (will be copied)
 * @param content      Result content (will be copied)
 * @param is_error     true if tool execution failed
 * @return             Allocated message, or NULL on OOM (PANIC)
 */
ik_message_t *ik_message_create_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                              const char *content, bool is_error);

/**
 * Convert database message to provider message
 *
 * Converts from ik_msg_t (database format) to ik_message_t (provider format).
 *
 * Mapping:
 * - "user" → USER role with text block
 * - "assistant" → ASSISTANT role with text block
 * - "tool_call" → ASSISTANT role with tool_call block (parses data_json)
 * - "tool_result" or "tool" → TOOL role with tool_result block (parses data_json)
 * - "system" → returns NULL (system prompts handled separately)
 *
 * @param ctx    Talloc parent context
 * @param db_msg Database message to convert (must not be NULL)
 * @return       Allocated message, NULL for system messages, or NULL on error
 */
ik_message_t *ik_message_from_db_msg(TALLOC_CTX *ctx, const ik_msg_t *db_msg);

#endif /* IK_MESSAGE_H */
```

**Include order (per style guide):**
1. Own header: N/A (this is the header)
2. Project headers: `error.h`, `providers/provider.h`, `db/message.h`
3. System headers: `<talloc.h>`

### 2. Create src/message.c

Implement the four functions. Key points:

**ik_message_create_text():**
```c
ik_message_t *ik_message_create_text(TALLOC_CTX *ctx, ik_role_t role, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = ik_content_block_text(msg, text);
    if (!block) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    msg->content_blocks[0] = *block;
    talloc_free(block); // Free the wrapper, we copied the struct

    return msg;
}
```

**ik_message_create_tool_call():**
- Similar to text, but role=ASSISTANT, use `ik_content_block_tool_call()`

**ik_message_create_tool_result():**
- Similar to text, but role=TOOL, use `ik_content_block_tool_result()`

**ik_message_from_db_msg():**
- Check kind field, return NULL for "system"
- For "tool_call": parse data_json to extract tool_call_id, name, arguments
- For "tool_result"/"tool": parse data_json to extract tool_call_id, output, success
- For other kinds: create text message from content field
- Use yyjson for JSON parsing (see fixed src/providers/request.c for pattern)

**Include order:**
```c
#include "message.h"

#include "msg.h"
#include "panic.h"
#include "providers/request.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
```

### 3. Update src/agent.h

Add new fields to `ik_agent_ctx_t` struct in the "Conversation state" section:

```c
    // Conversation state (per-agent)
    ik_openai_conversation_t *conversation;  // OLD - will be removed in Task 6

    // Provider-agnostic message storage (NEW - Task 0)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity
```

Add function declarations at the end of the public API section:

```c
/**
 * Add message to agent's message array
 *
 * Grows the array if needed using geometric expansion (2x).
 * Takes ownership of the message via talloc_steal().
 *
 * @param agent Agent context (must not be NULL)
 * @param msg   Message to add (must not be NULL)
 * @return      OK on success, ERR on allocation failure
 */
res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg);

/**
 * Clear all messages from agent
 *
 * Frees the messages array and resets count/capacity to zero.
 *
 * @param agent Agent context (must not be NULL)
 */
void ik_agent_clear_messages(ik_agent_ctx_t *agent);

/**
 * Clone messages from one agent to another
 *
 * Deep copies all messages and content blocks.
 *
 * @param dest Destination agent (must not be NULL)
 * @param src  Source agent (must not be NULL)
 * @return     OK on success, ERR on allocation failure
 */
res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src);
```

### 4. Update src/agent.c

**In `ik_agent_create()` function:**

Add initialization after the `conversation` field is created:

```c
    agent->conversation = ik_openai_conversation_create(agent);

    // NEW: Initialize provider-agnostic message storage
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
```

**Implement new functions at end of file:**

```c
res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg) {
    assert(agent != NULL); // LCOV_EXCL_BR_LINE
    assert(msg != NULL);   // LCOV_EXCL_BR_LINE

    // Grow array if at capacity
    if (agent->message_count >= agent->message_capacity) {
        size_t new_capacity = agent->message_capacity == 0 ? 16 : agent->message_capacity * 2;
        ik_message_t **new_array = talloc_realloc(agent, agent->messages, ik_message_t *, new_capacity);
        if (!new_array) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        agent->messages = new_array;
        agent->message_capacity = new_capacity;
    }

    // Steal ownership and add to array
    talloc_steal(agent, msg);
    agent->messages[agent->message_count++] = msg;

    return OK(msg);
}

void ik_agent_clear_messages(ik_agent_ctx_t *agent) {
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    if (agent->messages != NULL) {
        talloc_free(agent->messages);
    }
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
}

res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src) {
    assert(dest != NULL); // LCOV_EXCL_BR_LINE
    assert(src != NULL);  // LCOV_EXCL_BR_LINE

    if (src->message_count == 0) {
        return OK(NULL);
    }

    // Allocate array for destination
    dest->messages = talloc_array(dest, ik_message_t *, src->message_count);
    if (!dest->messages) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    dest->message_capacity = src->message_count;
    dest->message_count = 0; // Will increment as we add

    // Deep copy each message
    for (size_t i = 0; i < src->message_count; i++) {
        ik_message_t *src_msg = src->messages[i];

        // Allocate new message
        ik_message_t *dst_msg = talloc_zero(dest, ik_message_t);
        if (!dst_msg) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        dst_msg->role = src_msg->role;
        dst_msg->content_count = src_msg->content_count;
        dst_msg->provider_metadata = NULL; // Don't copy metadata

        // Allocate and copy content blocks
        dst_msg->content_blocks = talloc_array(dst_msg, ik_content_block_t, src_msg->content_count);
        if (!dst_msg->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        for (size_t j = 0; j < src_msg->content_count; j++) {
            ik_content_block_t *src_block = &src_msg->content_blocks[j];
            ik_content_block_t *dst_block = &dst_msg->content_blocks[j];

            dst_block->type = src_block->type;

            switch (src_block->type) {
                case IK_CONTENT_TEXT:
                    dst_block->data.text.text = talloc_strdup(dst_msg, src_block->data.text.text);
                    if (!dst_block->data.text.text) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    break;

                case IK_CONTENT_TOOL_CALL:
                    dst_block->data.tool_call.id = talloc_strdup(dst_msg, src_block->data.tool_call.id);
                    dst_block->data.tool_call.name = talloc_strdup(dst_msg, src_block->data.tool_call.name);
                    dst_block->data.tool_call.arguments = talloc_strdup(dst_msg, src_block->data.tool_call.arguments);
                    if (!dst_block->data.tool_call.id || !dst_block->data.tool_call.name ||
                        !dst_block->data.tool_call.arguments) {
                        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    }
                    break;

                case IK_CONTENT_TOOL_RESULT:
                    dst_block->data.tool_result.tool_call_id = talloc_strdup(dst_msg, src_block->data.tool_result.tool_call_id);
                    dst_block->data.tool_result.content = talloc_strdup(dst_msg, src_block->data.tool_result.content);
                    dst_block->data.tool_result.is_error = src_block->data.tool_result.is_error;
                    if (!dst_block->data.tool_result.tool_call_id || !dst_block->data.tool_result.content) {
                        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    }
                    break;

                case IK_CONTENT_THINKING:
                    dst_block->data.thinking.text = talloc_strdup(dst_msg, src_block->data.thinking.text);
                    if (!dst_block->data.thinking.text) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    break;

                default:
                    PANIC("Unknown content type: %d", src_block->type); // LCOV_EXCL_LINE
            }
        }

        dest->messages[dest->message_count++] = dst_msg;
    }

    return OK(dest->messages);
}
```

**Include updates:**
Add `#include "message.h"` to src/agent.c includes.

### 5. Update Makefile

Add `src/message.c` to `CLIENT_SOURCES` variable (in alphabetical order):

```makefile
CLIENT_SOURCES = \
    src/agent.c \
    ...
    src/message.c \
    ...
```

### 6. Create Basic Tests

Create `tests/unit/message/creation_test.c`:

```c
#include <check.h>
#include "message.h"
#include "error.h"
#include <talloc.h>

static TALLOC_CTX *test_ctx;

void setup(void) {
    test_ctx = talloc_new(NULL);
}

void teardown(void) {
    talloc_free(test_ctx);
}

START_TEST(test_message_create_text_user) {
    ik_message_t *msg = ik_message_create_text(test_ctx, IK_ROLE_USER, "Hello");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_USER);
    ck_assert_int_eq(msg->content_count, 1);
    ck_assert_ptr_nonnull(msg->content_blocks);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(msg->content_blocks[0].data.text.text, "Hello");
}
END_TEST

START_TEST(test_message_create_tool_call) {
    ik_message_t *msg = ik_message_create_tool_call(test_ctx, "call_123", "read_file", "{\"path\":\"foo\"}");

    ck_assert_ptr_nonnull(msg);
    ck_assert_int_eq(msg->role, IK_ROLE_ASSISTANT);
    ck_assert_int_eq(msg->content_count, 1);
    ck_assert_int_eq(msg->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.name, "read_file");
    ck_assert_str_eq(msg->content_blocks[0].data.tool_call.arguments, "{\"path\":\"foo\"}");
}
END_TEST

Suite *suite(void) {
    Suite *s = suite_create("Message Creation");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_message_create_text_user);
    tcase_add_test(tc, test_message_create_tool_call);

    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    Suite *s = suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed;
}
```

**Add to Makefile:**
```makefile
UNIT_TESTS += tests/unit/message/creation_test
```

## Build Verification

After implementation:

```bash
# Clean build
make clean
make all

# Expected: Builds successfully, no errors

# Run tests
make check

# Expected: All existing tests still pass + new message tests pass
```

## Postconditions

- [ ] `scratch/task-0-assessment.txt` created documenting current state
- [ ] `src/message.h` created with 4 public functions
- [ ] `src/message.c` created with implementations
- [ ] `src/agent.h` has new fields: messages, message_count, message_capacity
- [ ] `src/agent.h` has 3 new function declarations
- [ ] `src/agent.c` initializes new fields in ik_agent_create()
- [ ] `src/agent.c` has 3 new functions: add_message, clear_messages, clone_messages
- [ ] `Makefile` includes src/message.c in CLIENT_SOURCES
- [ ] `tests/unit/message/creation_test.c` created
- [ ] `make clean && make all` succeeds
- [ ] `make check` passes (all old + new tests)
- [ ] No compiler warnings

## Success Criteria

After this task:
1. Foundation infrastructure exists but is UNPOPULATED
2. Old `agent->conversation` field still exists and is used
3. New `agent->messages` field exists but is NULL/empty
4. Message creation functions exist and are tested
5. Agent management functions exist and are tested
6. All existing tests still pass (no behavioral changes)
7. Build is clean with no warnings
8. Ready for Task 1-9 to wire up the dual-mode population

**What's NOT done yet:**
- Messages are NOT being added to the new array (that's Tasks 1-4)
- Request builder still reads from old storage (changes in Task 5)
- Old storage is NOT removed (that's Task 6)
- Legacy files are NOT deleted (that's Tasks 7-8)

This task only creates the infrastructure. Subsequent tasks will use it.
