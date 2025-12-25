# Task: Delete Legacy OpenAI Files (Part 2: Request Serialization Refactoring)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/extended
**Depends on:** delete-legacy-files-part1.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Refactor OpenAI request builders to serialize directly from `ik_request_t` to JSON, bypassing the deleted shim layer. Update provider.h to use provider-agnostic tool choice enum. This completes the legacy OpenAI cleanup.

## Pre-Read

**Skills:**
- `/load errors` - Result types and error handling
- `/load naming` - Naming conventions
- `/load style` - Code style

**Source Files to Read:**
- `src/providers/openai/request_chat.c` - Chat API request builder (currently uses shim)
- `src/providers/openai/request_responses.c` - Responses API request builder (currently uses shim)
- `src/providers/provider.h` - Provider interface definitions
- `src/providers/anthropic/request_messages.c` - Reference for similar direct serialization pattern

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

### 1. Update src/providers/openai/request_chat.c - Direct JSON Serialization

**Context:** The shim layer converted ik_request_t → ik_openai_conversation_t → JSON. Now go directly from ik_request_t → JSON.

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

### 2. Update src/providers/openai/request_responses.c - Direct JSON Serialization

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

### 3. Update src/providers/provider.h

**Uncomment tool choice enum - search for commented ik_tool_choice_t:**

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

**Update ik_request struct - search for tool_choice_mode field:**

Change:
```c
int tool_choice_mode;  /* Tool choice mode (temporarily int during coexistence) */
```

To:
```c
ik_tool_choice_t tool_choice_mode;  /* Tool choice mode */
```

### 4. Update src/providers/factory.c

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

All integration tests should still pass with shim removed:

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

After refactoring:

```bash
make clean
make all
```

Should compile without errors. Verify:
- No undefined references to `ik_openai_*` functions
- No missing includes
- No linker errors
- No warnings about unused shim

## Postconditions

- [ ] `src/providers/openai/request_chat.c` works without shim
- [ ] `src/providers/openai/request_responses.c` works without shim
- [ ] Tool choice enum uncommented in `provider.h`
- [ ] Verification script passes
- [ ] `make clean && make all` succeeds
- [ ] `make check` passes
- [ ] No compiler or linker errors
- [ ] All provider tests pass

## Success Criteria

After this task:
1. All provider code works directly with `ik_message_t` format
2. Direct JSON serialization replaces shim layer
3. Build completes successfully
4. All tests pass
5. No legacy OpenAI coupling remains in codebase
6. Provider-agnostic tool choice enum in use
