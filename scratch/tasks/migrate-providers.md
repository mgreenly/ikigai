# Task: Migrate Provider Request Building to New Storage

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** migrate-restore.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Update provider request builder to read from new `agent->messages` array instead of old `agent->conversation`. This is the final migration step - after this task, new storage becomes authoritative. Update mock wrappers to match new API.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load naming` - Naming conventions
- `/load style` - Code style
- `/load tdd` - Test-driven development
- `/load mocking` - Mockable wrapper patterns

**Source Files to Read:**
- `src/providers/request.h` - Request builder API
- `src/providers/request.c` - `ik_request_build_from_conversation()` implementation - locate function that builds request from agent
- `src/providers/provider.h` - Complete struct definitions:
  - `struct ik_request`: messages (ik_message_t* array), message_count, model, system_prompt, thinking, tools, tool_count, max_output_tokens
  - `struct ik_message`: role, content_blocks (array pointer), content_count, provider_metadata
  - `struct ik_content_block`: type discriminator + union (text/tool_call/tool_result/thinking)
- `src/wrapper.c` - Mockable wrapper implementations
- `src/wrapper_internal.h` - Mockable wrapper declarations

**Test Files:**
- `tests/unit/providers/request_test.c` - Request building tests
- `tests/integration/providers/anthropic/basic_test.c` - Provider integration tests

## Error Handling Policy

**Memory Allocation Failures:**
- All talloc allocations: PANIC with LCOV_EXCL_BR_LINE
- Rationale: OOM is unrecoverable, panic is appropriate

**Validation Failures:**
- Return ERR allocated on parent context (not on object being freed)
- Example: `return ERR(parent_ctx, INVALID_ARG, "message")`

**During Dual-Mode (Tasks 1-4):**
- Old API calls succeed: continue normally
- New API calls fail: log error, continue (old API is authoritative)
- Pattern: `if (is_err(&res)) { ik_log_error("Failed: %s", res.err->msg); }`

**After Migration (Tasks 5-8):**
- New API calls fail: propagate error immediately
- Pattern: `if (is_err(&res)) { return res; }`

**Assertions:**
- NULL pointer checks: `assert(ptr != NULL)` with LCOV_EXCL_BR_LINE
- Only for programmer errors, never for runtime conditions

## Implementation

### 1. Update src/providers/request.c - Switch to New Storage

**Location:** `ik_request_build_from_conversation()` function - search for this function

**Current Code:**
```c
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent_ptr, ik_request_t **out) {
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)agent_ptr;

    // ... validation and request creation ...

    /* Iterate conversation messages and add to request */
    if (agent->conversation != NULL) {
        for (size_t i = 0; i < agent->conversation->message_count; i++) {
            ik_msg_t *msg = agent->conversation->messages[i];
            // ... convert msg to request format ...
        }
    }
}
```

**New Code - Use agent->messages:**
```c
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent_ptr, ik_request_t **out) {
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)agent_ptr;

    /* Validate agent has required fields */
    if (agent->model == NULL || strlen(agent->model) == 0) {
        return ERR(ctx, INVALID_ARG, "No model configured");
    }

    /* Create request with agent's model */
    ik_request_t *req = NULL;
    res_t res = ik_request_create(ctx, agent->model, &req);
    if (is_err(&res)) return res;

    /* Set thinking level from agent */
    ik_request_set_thinking(req, (ik_thinking_level_t)agent->thinking_level, false);

    /* Iterate NEW message storage and add to request */
    if (agent->messages != NULL) {
        for (size_t i = 0; i < agent->message_count; i++) {
            ik_message_t *msg = agent->messages[i];
            if (msg == NULL) continue;

            /* Add message directly to request - already in provider format */
            res = ik_request_add_message_direct(req, msg);
            if (is_err(&res)) {
                talloc_free(req);
                return res;
            }
        }
    }

    /* Add standard tool definitions (glob, file_read, grep, file_write, bash) */
    /* Note: ik_tool_schema_def_t is internal to request.c - see existing implementation */
    /* The public API is ik_request_add_tool(req, name, description, params_json, strict) */
    /* Tool schema definitions exist in request.c - search for glob_schema_def, file_read_schema_def, etc. */
    const ik_tool_schema_def_t *tool_defs[] = {
        &glob_schema_def,
        &file_read_schema_def,
        &grep_schema_def,
        &file_write_schema_def,
        &bash_schema_def
    };

    for (size_t i = 0; i < 5; i++) {
        char *params_json = build_tool_parameters_json(req, tool_defs[i]);
        res = ik_request_add_tool(req, tool_defs[i]->name, tool_defs[i]->description, params_json, false);
        if (is_err(&res)) {
            talloc_free(req);
            return res;
        }
    }

    *out = req;
    return OK(req);
}
```

**Key Changes:**
- Read from `agent->messages` instead of `agent->conversation`
- Use `agent->message_count` instead of `agent->conversation->message_count`
- Messages are already `ik_message_t*` (provider format), no conversion needed
- Use `ik_request_add_message_direct()` instead of old conversion logic

### 2. Add ik_request_add_message_direct() Helper

**Location:** Add to `src/providers/request.c` before `ik_request_build_from_conversation()`

**Function Signature:**
```c
static res_t ik_request_add_message_direct(ik_request_t *req, const ik_message_t *msg);
```

**Purpose:** Deep copy an existing message and all its content blocks into the request's message array.

**Parameters:**
- `req` - Request to add message to (must not be NULL)
- `msg` - Message to copy (must not be NULL, already in provider format)

**Behavior:**
- Grow request->messages array to accommodate new message (use geometric growth or realloc)
- Deep copy the message struct: role, content_count
- Deep copy all content blocks and their string fields (id, name, arguments, text, etc.)
- Set provider_metadata to NULL (don't copy response metadata into requests)
- Handle all content types: IK_CONTENT_TEXT, IK_CONTENT_TOOL_CALL, IK_CONTENT_TOOL_RESULT, IK_CONTENT_THINKING
- Increment request->message_count after successful add

**Deep Copy Algorithm (Step-by-Step):**

1. Allocate new message struct in request context:
   ```c
   ik_message_t *copy = talloc(req, ik_message_t);
   copy->role = msg->role;
   copy->content_count = msg->content_count;
   copy->provider_metadata = NULL;  // Don't copy response metadata
   ```

2. Allocate content blocks array:
   ```c
   copy->content_blocks = talloc_array(copy, ik_content_block_t, msg->content_count);
   ```

3. For each content block, deep copy based on type:
   ```c
   for (size_t i = 0; i < msg->content_count; i++) {
       ik_content_block_t *src = &msg->content_blocks[i];
       ik_content_block_t *dst = &copy->content_blocks[i];
       dst->type = src->type;

       switch (src->type) {
       case IK_CONTENT_TEXT:
           dst->data.text.text = talloc_strdup(copy, src->data.text.text);
           break;

       case IK_CONTENT_TOOL_CALL:
           dst->data.tool_call.id = talloc_strdup(copy, src->data.tool_call.id);
           dst->data.tool_call.name = talloc_strdup(copy, src->data.tool_call.name);
           dst->data.tool_call.arguments = talloc_strdup(copy, src->data.tool_call.arguments);
           break;

       case IK_CONTENT_TOOL_RESULT:
           dst->data.tool_result.tool_call_id = talloc_strdup(copy, src->data.tool_result.tool_call_id);
           dst->data.tool_result.content = talloc_strdup(copy, src->data.tool_result.content);
           dst->data.tool_result.is_error = src->data.tool_result.is_error;
           break;

       case IK_CONTENT_THINKING:
           dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
           break;

       default:
           PANIC("Unknown content type: %d", src->type);  // LCOV_EXCL_LINE
       }
   }
   ```

4. Add copy to request->messages array (resize if needed)

**Error Handling:**
- Preconditions: assert(req != NULL) and assert(msg != NULL) with LCOV_EXCL_BR_LINE
- Allocate errors on PARENT context, not req context (to avoid use-after-free if req is freed)
- PANIC on OOM for talloc allocations (mark with LCOV_EXCL_BR_LINE)
- Switch default case: PANIC("Unknown content type") with LCOV_EXCL_LINE

**Memory Ownership:**
- All allocations are children of req context
- All string fields copied with talloc_strdup()
- Content blocks allocated as talloc_array() under req

**Return:**
- OK(copy) where copy points to the newly added message in request array
- ERR for validation failures (allocate error on parent context)

### 3. Update src/wrapper.c - Mock Wrappers

**Current Wrappers:**
```c
MOCKABLE_IMPL(res_t, ik_openai_conversation_add_msg_,
              (ik_openai_conversation_t *conv, ik_msg_t *msg))
{
    return ik_openai_conversation_add_msg(conv, msg);
}
```

**New Wrappers:**
```c
MOCKABLE_IMPL(res_t, ik_agent_add_message_,
              (ik_agent_ctx_t *agent, ik_message_t *msg))
{
    return ik_agent_add_message(agent, msg);
}
```

**Also Update src/wrapper_internal.h:**
```c
MOCKABLE_DECL(res_t, ik_agent_add_message_, (ik_agent_ctx_t *agent, ik_message_t *msg));
```

**Note:** Keep old `ik_openai_conversation_add_msg_` wrapper for now - will be removed in later task when we delete all legacy code.

## Test Requirements

### Update Existing Tests

**tests/unit/providers/request_test.c:**
- Update tests to use `agent->messages` instead of `agent->conversation`
- Verify request built from new storage has correct message count
- Verify content blocks transferred correctly

**tests/integration/providers/anthropic/basic_test.c:**
- Verify end-to-end flow: add messages → build request → send to provider
- Check request contains all messages from `agent->messages`

**tests/integration/providers/openai/basic_test.c:**
- Same verification for OpenAI provider

**tests/integration/providers/google/basic_test.c:**
- Same verification for Google provider

### Create New Tests

**tests/unit/providers/request_direct_test.c:**

**Test Cases:**
- `test_add_message_direct_text` - Add text message, verify content
- `test_add_message_direct_tool_call` - Add tool call, verify all fields copied
- `test_add_message_direct_tool_result` - Add tool result, verify fields
- `test_add_message_direct_multiple_blocks` - Message with multiple content blocks
- `test_add_message_direct_deep_copy` - Verify deep copy (modify original, check copy)

## Postconditions

- [ ] `src/providers/request.c` reads from `agent->messages`
- [ ] `ik_request_add_message_direct()` implemented and tested
- [ ] No references to `agent->conversation` in request.c
- [ ] Wrapper mocks updated for new API
- [ ] Existing provider tests updated
- [ ] New direct message tests created and passing
- [ ] All three providers (Anthropic, OpenAI, Google) work with new storage
- [ ] `make check` passes
- [ ] No compiler warnings

## Success Criteria

After this task:
1. Provider request building uses ONLY new `agent->messages` storage
2. Old `agent->conversation` is no longer read by any provider code
3. New storage is now authoritative for all LLM requests
4. All three providers work correctly with new format
5. Tests verify messages transferred correctly to requests
6. Ready to remove old conversation field in next task
