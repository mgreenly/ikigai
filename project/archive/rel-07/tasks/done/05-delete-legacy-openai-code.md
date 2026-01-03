# Task: Delete Legacy OpenAI Code

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** 04-migrate-restore-to-new-api.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Delete legacy `src/openai/` directory (19 files), shim layer files, and legacy test files. Update Makefile to remove deleted sources. Update OpenAI provider request builders to serialize directly from `ik_request_t` to JSON without shim layer.

## Pre-Read

**Skills:**
- `/load makefile` - Build system
- `/load source-code` - File organization
- `/load git` - Git operations

**Files to Review:**
- `Makefile` - Source file lists
- `src/providers/openai/request_chat.c` - Currently uses shim (will refactor)
- `src/providers/openai/request_responses.c` - Currently uses shim (will refactor)
- `src/providers/anthropic/request_messages.c` - Reference for direct serialization pattern

## Implementation

### 1. Delete src/openai/ Directory

**Files to delete (19 total):**
- Client core: client.c, client.h, client_msg.c, client_serialize.c
- Multi-handle: client_multi.c, client_multi.h, client_multi_internal.h, client_multi_request.c, client_multi_callbacks.c, client_multi_callbacks.h
- HTTP/Streaming: http_handler.c, http_handler.h, http_handler_internal.h, sse_parser.c, sse_parser.h
- Tool choice: tool_choice.c, tool_choice.h
- Build artifacts: *.o files

**Command:**
```bash
rm -rf src/openai/
```

### 2. Delete Shim Layer

**Files to delete:**
```bash
rm -f src/providers/openai/shim.c
rm -f src/providers/openai/shim.h
```

### 3. Delete Legacy Tests

**If these exist:**
```bash
rm -rf tests/unit/openai/
```

### 4. Update Makefile

**Find:** `CLIENT_SOURCES` variable

**Remove these lines:**
```makefile
src/openai/client.c \
src/openai/client_msg.c \
src/openai/client_serialize.c \
src/openai/client_multi.c \
src/openai/client_multi_request.c \
src/openai/client_multi_callbacks.c \
src/openai/http_handler.c \
src/openai/sse_parser.c \
src/openai/tool_choice.c \
src/providers/openai/shim.c \
```

**Add this line:**
```makefile
src/providers/openai/serialize.c \
```

**Remove test targets** for deleted tests (search for `tests/unit/openai`).

### 5. Update src/providers/openai/request_chat.c

**Remove shim includes:**
```c
#include "openai/client.h"         // DELETE
#include "providers/openai/shim.h" // DELETE
```

**Find and delete shim usage:**

OLD pattern:
```c
ik_openai_conversation_t *conv = NULL;
res_t res = ik_openai_shim_build_conversation(ctx, request, &conv);
if (is_err(&res)) return res;
```

**Replace with direct JSON serialization:**

```c
/* Build JSON directly from request->messages array */
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

yyjson_mut_val *root = yyjson_mut_obj(doc);
if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

/* Add model */
yyjson_mut_obj_add_str(doc, root, "model", request->model);

/* Add messages array */
yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
if (messages_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

for (size_t i = 0; i < request->message_count; i++) {
    ik_message_t *msg = &request->messages[i];
    yyjson_mut_val *msg_obj = ik_openai_serialize_message(doc, msg);
    yyjson_mut_arr_append(messages_arr, msg_obj);
}
yyjson_mut_obj_add_val(doc, root, "messages", messages_arr);

/* Add tools if present */
if (request->tool_count > 0) {
    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    for (size_t i = 0; i < request->tool_count; i++) {
        yyjson_mut_val *tool_obj = serialize_tool_to_json(doc, &request->tools[i]);
        yyjson_mut_arr_append(tools_arr, tool_obj);
    }
    yyjson_mut_obj_add_val(doc, root, "tools", tools_arr);
}

/* Add other request fields (temperature, max_tokens, etc.) */
/* ... existing serialization code ... */
```

**Add include:**
```c
#include "providers/openai/serialize.h"
```

### 5a. Create src/providers/openai/serialize.c and serialize.h

**Purpose:** Shared serialization helpers for OpenAI JSON format (used by both chat and responses endpoints).

**src/providers/openai/serialize.h:**
```c
#ifndef IK_PROVIDERS_OPENAI_SERIALIZE_H
#define IK_PROVIDERS_OPENAI_SERIALIZE_H

#include "providers/provider.h"

#include <yyjson.h>

/**
 * Serialize message to OpenAI Chat API JSON format
 */
yyjson_mut_val *ik_openai_serialize_message(yyjson_mut_doc *doc, const ik_message_t *msg);

#endif // IK_PROVIDERS_OPENAI_SERIALIZE_H
```

**src/providers/openai/serialize.c:**

```c
#include "providers/openai/serialize.h"

#include "panic.h"

#include <assert.h>

yyjson_mut_val *ik_openai_serialize_message(yyjson_mut_doc *doc, const ik_message_t *msg)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE
    assert(msg != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    if (msg_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Add role */
    const char *role_str = NULL;
    switch (msg->role) {
    case IK_ROLE_USER:
        role_str = "user";
        break;
    case IK_ROLE_ASSISTANT:
        role_str = "assistant";
        break;
    case IK_ROLE_TOOL:
        role_str = "tool";
        break;
    default:
        PANIC("Unknown role: %d", msg->role);  // LCOV_EXCL_LINE
    }
    yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str);

    /* Serialize content blocks */
    for (size_t i = 0; i < msg->content_count; i++) {
        ik_content_block_t *block = &msg->content_blocks[i];

        switch (block->type) {
        case IK_CONTENT_TEXT:
            yyjson_mut_obj_add_str(doc, msg_obj, "content", block->data.text.text);
            break;

        case IK_CONTENT_TOOL_CALL: {
            /* Set content to null when tool_calls present */
            yyjson_mut_obj_add_null(doc, msg_obj, "content");

            /* Build tool_calls array */
            yyjson_mut_val *tool_calls = yyjson_mut_arr(doc);
            yyjson_mut_val *tc_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, tc_obj, "id", block->data.tool_call.id);
            yyjson_mut_obj_add_str(doc, tc_obj, "type", "function");

            yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, func_obj, "name", block->data.tool_call.name);
            yyjson_mut_obj_add_str(doc, func_obj, "arguments", block->data.tool_call.arguments);
            yyjson_mut_obj_add_val(doc, tc_obj, "function", func_obj);

            yyjson_mut_arr_append(tool_calls, tc_obj);
            yyjson_mut_obj_add_val(doc, msg_obj, "tool_calls", tool_calls);
            break;
        }

        case IK_CONTENT_TOOL_RESULT:
            yyjson_mut_obj_add_str(doc, msg_obj, "tool_call_id", block->data.tool_result.tool_call_id);
            yyjson_mut_obj_add_str(doc, msg_obj, "content", block->data.tool_result.content);
            break;

        case IK_CONTENT_THINKING:
            /* For o1 models: thinking not included in request (response only) */
            /* For other models: treat as text or skip */
            break;

        default:
            PANIC("Unknown content type: %d", block->type);  // LCOV_EXCL_LINE
        }
    }

    return msg_obj;
}
```

**Reference:** See `src/providers/anthropic/request_messages.c` for similar pattern.

### 6. Update src/providers/openai/request_responses.c

Apply same transformation as request_chat.c:
- Remove shim includes
- Delete shim usage
- Add direct JSON serialization using `ik_openai_serialize_message()` from serialize.h
- Add include: `#include "providers/openai/serialize.h"`

**Note:** Responses API uses `reasoning_effort` instead of `temperature`.

### 7. Update src/providers/openai/openai.c

**Remove shim include:**
```c
#include "providers/openai/shim.h"  // DELETE THIS LINE
```

**Remove any shim function calls** if present.

## Build Verification

```bash
make clean
make all
```

**Expected:**
- Build succeeds
- No undefined references to `ik_openai_*` functions
- No references to shim layer
- No linker errors

```bash
make check
```

**Expected:**
- All tests pass
- OpenAI provider works without shim

## Grep Verification

After deletions, these should return EMPTY:

```bash
grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h"
grep -r "ik_openai_msg_create" src/ --include="*.c" --include="*.h"
grep -r "#include.*openai/client" src/ --include="*.c" --include="*.h"
grep -r "#include.*shim\.h" src/ --include="*.c" --include="*.h"
```

## Postconditions

- [ ] `src/openai/` directory deleted (19 files)
- [ ] Shim files deleted (shim.c, shim.h)
- [ ] Legacy tests deleted
- [ ] New serialize.c and serialize.h created
- [ ] Makefile updated (legacy sources removed, serialize.c added)
- [ ] request_chat.c uses `ik_openai_serialize_message()` from serialize.h
- [ ] request_responses.c uses `ik_openai_serialize_message()` from serialize.h
- [ ] openai.c has no shim includes
- [ ] Build succeeds
- [ ] `make check` passes
- [ ] Grep checks return empty

## Success Criteria

After this task:
1. All legacy OpenAI code deleted (~6500 lines removed)
2. New shared serialization module created (serialize.c/h)
3. OpenAI provider works with direct JSON serialization (no shim)
4. No static helper functions (follows style guide)
5. Build and tests pass
6. Net reduction: 19 files deleted, 2 created = 17 files smaller
7. Ready for final verification
