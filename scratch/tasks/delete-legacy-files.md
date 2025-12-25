# Task: Delete Legacy OpenAI Files and Shim Layer

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** remove-legacy-conversation.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Delete entire `src/openai/` directory (19 files) and shim layer in `src/providers/openai/`. Update Makefile to remove legacy source files. Legacy code is now completely disconnected and can be safely removed.

## Pre-Read

**Skills:**
- `/load makefile` - Build system and source file management
- `/load source-code` - File map to understand dependencies
- `/load git` - Git operations and commit practices

**Files to Review:**
- `Makefile` - Source file lists and build rules
- `scratch/legacy-openai-removal-checklist.md` - Complete inventory of files to delete

## Implementation

### 1. Delete src/openai/ Directory

**Files to Delete (19 total):**

**Client Core:**
- `src/openai/client.c`
- `src/openai/client.h`
- `src/openai/client_msg.c`
- `src/openai/client_serialize.c`

**Multi-handle Manager:**
- `src/openai/client_multi.c`
- `src/openai/client_multi.h`
- `src/openai/client_multi_internal.h`
- `src/openai/client_multi_request.c`
- `src/openai/client_multi_callbacks.c`
- `src/openai/client_multi_callbacks.h`

**HTTP/Streaming:**
- `src/openai/http_handler.c`
- `src/openai/http_handler.h`
- `src/openai/http_handler_internal.h`
- `src/openai/sse_parser.c`
- `src/openai/sse_parser.h`

**Tool Choice:**
- `src/openai/tool_choice.c`
- `src/openai/tool_choice.h`

**Build Artifacts:**
- `src/openai/client_multi_request.o`
- `src/openai/client.o`

**Command:**
```bash
rm -rf src/openai/
```

### 2. Delete Shim Layer in src/providers/openai/

**Files to Delete:**
- `src/providers/openai/shim.c`
- `src/providers/openai/shim.h`

**Command:**
```bash
rm src/providers/openai/shim.c
rm src/providers/openai/shim.h
```

### 3. Update Makefile - Remove Legacy Sources

**Location:** Find `CLIENT_SOURCES` variable

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

**Also remove from any test-specific source lists if present.**

### 4. Update src/providers/openai/openai.c

**Remove shim includes:**
```c
#include "providers/openai/shim.h"  // DELETE THIS LINE
```

**Remove any calls to shim functions:**
- `ik_openai_shim_build_conversation()`
- `ik_openai_shim_map_finish_reason()`

These functions should no longer be needed since we're working directly with `ik_message_t` arrays.

### 5. Update src/providers/openai/request_chat.c - Remove Shim Usage

**Context:** The shim layer converts ik_request_t → ik_openai_conversation_t → JSON. After this task, go directly from ik_request_t → JSON.

**Current Pattern (uses shim):**
```c
// Uses shim to convert to legacy format
ik_openai_conversation_t *conv = NULL;
res_t res = ik_openai_shim_build_conversation(ctx, request, &conv);
if (is_err(&res)) return res;

// Then serialize legacy conversation to JSON
// ... existing JSON serialization code ...
```

**New Pattern (direct serialization):**
```c
// Build JSON directly from request->messages array
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
yyjson_mut_val *root = yyjson_mut_obj(doc);

// Add model
yyjson_mut_obj_add_str(doc, root, "model", request->model);

// Add system prompt if present
if (request->system_prompt != NULL) {
    // System prompt handled via messages array or top-level field depending on API
}

// Add messages array
yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
for (size_t i = 0; i < request->message_count; i++) {
    ik_message_t *msg = &request->messages[i];
    yyjson_mut_val *msg_obj = serialize_message_to_json(doc, msg);
    yyjson_mut_arr_append(messages_arr, msg_obj);
}
yyjson_mut_obj_add_val(doc, root, "messages", messages_arr);

// ... add other request fields (temperature, max_tokens, tools, etc.) ...
```

**Helper Function Needed:**
```c
static yyjson_mut_val *serialize_message_to_json(yyjson_mut_doc *doc, const ik_message_t *msg);
```

**Complete OpenAI Chat API Message Formats:**

1. **User message (simple text):**
```json
{
  "role": "user",
  "content": "What is the capital of France?"
}
```

2. **Assistant message (text response):**
```json
{
  "role": "assistant",
  "content": "The capital of France is Paris."
}
```

3. **Assistant message (with tool calls):**
```json
{
  "role": "assistant",
  "content": null,
  "tool_calls": [
    {
      "id": "call_abc123",
      "type": "function",
      "function": {
        "name": "file_read",
        "arguments": "{\"path\":\"config.json\"}"
      }
    }
  ]
}
```

4. **Tool result message:**
```json
{
  "role": "tool",
  "tool_call_id": "call_abc123",
  "content": "{\"success\": true, \"data\": \"file contents here\"}"
}
```

5. **System message (GPT-4, GPT-3.5):**
```json
{
  "role": "system",
  "content": "You are a helpful assistant."
}
```

**Note:** o1 models do NOT support system messages in the messages array. For o1, system prompts must be included as a developer message or omitted.

**Content Block to JSON Mapping:**

```c
// IK_CONTENT_TEXT (role=USER or ASSISTANT):
{"role": "user"|"assistant", "content": block->data.text.text}

// IK_CONTENT_TOOL_CALL (role=ASSISTANT):
{
  "role": "assistant",
  "content": null,  // Important: set to null when tool_calls present
  "tool_calls": [
    {
      "id": block->data.tool_call.id,
      "type": "function",
      "function": {
        "name": block->data.tool_call.name,
        "arguments": block->data.tool_call.arguments  // Already JSON string
      }
    }
  ]
}

// IK_CONTENT_TOOL_RESULT (role=TOOL):
{
  "role": "tool",
  "tool_call_id": block->data.tool_result.tool_call_id,
  "content": block->data.tool_result.content
}

// IK_CONTENT_THINKING:
// For o1 models: Not included in request (thinking is in response only)
// For other models: Treat as text content or skip
```

**Multiple Content Blocks in One Message:**
- If message has multiple IK_CONTENT_TEXT blocks: concatenate into single "content" field
- If message has multiple IK_CONTENT_TOOL_CALL blocks: all go in "tool_calls" array
- Mixed types: Not typical, handle by building appropriate JSON structure

**Remove includes:**
```c
#include "openai/client.h"        // DELETE
#include "providers/openai/shim.h" // DELETE
```

**Reference:**
- OpenAI Chat API docs: https://platform.openai.com/docs/api-reference/chat
- See existing `src/providers/anthropic/request_messages.c` for similar pattern

### 6. Update src/providers/openai/request_responses.c - Remove Shim Usage

**Context:** Responses API (for o1 models) has different JSON structure than Chat API.

**Responses API Format:**
```json
{
  "model": "o1-preview",
  "messages": [...],
  "reasoning_effort": "medium"  // Instead of temperature
}
```

**Pattern:** Same as request_chat.c but:
- Use `reasoning_effort` field instead of `temperature`
- System prompts handled differently (may not be supported)
- No streaming support
- Different response structure

**Reference:**
- OpenAI Responses API: different from Chat API
- See `src/providers/openai/reasoning.c` for model detection
- Use `ik_openai_prefer_responses_api()` to determine which API to use

### 7. Delete Legacy Test Files

**If these exist, delete them:**
- `tests/unit/openai/client_test.c`
- `tests/unit/openai/client_msg_test.c`
- `tests/unit/openai/http_handler_test.c`
- `tests/unit/openai/sse_parser_test.c`
- Any other tests in `tests/unit/openai/` directory

**Command:**
```bash
rm -rf tests/unit/openai/
```

**Update Makefile:**
Remove test targets for deleted tests.

### 8. Update src/providers/provider.h

**Uncomment tool choice enum (line 84-91):**

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

This was commented out to avoid conflict with `openai/tool_choice.h`. Now that legacy code is deleted, we can use the provider-agnostic enum.

**Update ik_request struct (line 209):**

Change:
```c
int tool_choice_mode;  /* Tool choice mode (temporarily int during coexistence) */
```

To:
```c
ik_tool_choice_t tool_choice_mode;  /* Tool choice mode */
```

### 9. Update src/providers/factory.c

**Remove legacy OpenAI create:**

If `ik_openai_create()` is defined in the old `src/openai/client.c`, it's now gone. The new OpenAI provider should be in `src/providers/openai/openai.c`.

**Verify factory calls correct function:**
```c
// Should call new provider implementation
extern ik_provider_t *ik_openai_provider_create(TALLOC_CTX *ctx, ik_logger_t *logger);
```

Not the old `ik_openai_create()` from legacy client.

**Update src/providers/stubs.h** if it has old declarations.

## Test Requirements

### Update Existing Tests

All integration tests should still pass with legacy code deleted:

**tests/integration/providers/openai/basic_test.c:**
- Verify still compiles and runs
- Uses new provider implementation only
- No references to legacy types

**tests/integration/providers/openai/streaming_test.c:**
- Verify streaming still works
- No legacy SSE parser references

### Verification Script

Create `tests/verify_no_legacy.sh`:

```bash
#!/bin/bash
set -e

echo "Verifying legacy code removal..."

# Should not exist
if [ -d "src/openai" ]; then
    echo "ERROR: src/openai/ still exists"
    exit 1
fi

if [ -f "src/providers/openai/shim.c" ]; then
    echo "ERROR: shim.c still exists"
    exit 1
fi

# Should find nothing
if grep -r "ik_openai_conversation_t" src/ --include="*.c" --include="*.h" 2>/dev/null; then
    echo "ERROR: Found ik_openai_conversation_t references"
    exit 1
fi

if grep -r "ik_openai_msg_create" src/ --include="*.c" --include="*.h" 2>/dev/null; then
    echo "ERROR: Found ik_openai_msg_create references"
    exit 1
fi

if grep -r "#include.*openai/client" src/ --include="*.c" --include="*.h" 2>/dev/null; then
    echo "ERROR: Found openai/client.h includes"
    exit 1
fi

echo "✓ All legacy code removed successfully"
```

Make executable and run:
```bash
chmod +x tests/verify_no_legacy.sh
./tests/verify_no_legacy.sh
```

## Build Verification

After deletions:

```bash
make clean
make all
```

Should compile without errors. Verify:
- No undefined references to `ik_openai_*` functions
- No missing includes
- No linker errors

## Postconditions

- [ ] `src/openai/` directory deleted (19 files removed)
- [ ] `src/providers/openai/shim.c` and `shim.h` deleted
- [ ] Makefile updated (legacy sources removed)
- [ ] `src/providers/openai/request_chat.c` works without shim
- [ ] `src/providers/openai/request_responses.c` works without shim
- [ ] Tool choice enum uncommented in `provider.h`
- [ ] `tests/unit/openai/` deleted if it exists
- [ ] Verification script passes
- [ ] `make clean && make all` succeeds
- [ ] `make check` passes
- [ ] No compiler or linker errors

## Success Criteria

After this task:
1. Legacy `src/openai/` directory completely removed
2. Shim layer completely removed
3. All provider code works directly with `ik_message_t` format
4. Build completes successfully
5. All tests pass
6. Codebase is 21 files smaller (~8000 lines removed)
7. No legacy OpenAI coupling remains in codebase
