# Task: Implement Google Provider

**Phase:** 5 - Google
**Depends on:** 01-provider-types, 02-shared-utilities, 03-request-builders

## Objective

Create native Google (Gemini) provider implementing the vtable interface.

## Deliverables

1. Create `src/providers/google/` directory:
   - `google.h` - Public interface
   - `adapter.c` - Vtable implementation
   - `client.c` - Request serialization
   - `streaming.c` - SSE event handling

2. Implement vtable:
   - `ik_google_create()` - Factory function
   - `ik_google_send()` - Non-streaming requests
   - `ik_google_stream()` - Streaming

3. Implement request transformation:
   - System prompt → `systemInstruction.parts[]`
   - Messages → `contents[]` with role mapping
   - Thinking level → `thinkingBudget` or `thinkingLevel`
   - Tools → `tools[].functionDeclarations`

4. Implement response parsing:
   - `candidates[0].content.parts[]` → content blocks
   - `thought: true` → `IK_CONTENT_THINKING`
   - `functionCall` → `IK_CONTENT_TOOL_CALL` (generate UUID)
   - `usageMetadata` → `ik_usage_t`

5. Implement tool call ID generation:
   - Google doesn't provide IDs
   - Generate 22-char base64url UUIDs

6. Implement thinking level mapping:
   - Gemini 2.5: `thinkingBudget` (token count)
   - Gemini 3: `thinkingLevel` (LOW/HIGH)

7. Handle thought signatures (Gemini 3):
   - Store in `provider_data`
   - Resubmit in subsequent requests

## Reference

- `scratch/plan/transformation.md` - Google Transformation
- `scratch/plan/thinking-abstraction.md` - Google section
- `scratch/research/google.md` - API details

## Verification

- Can send non-streaming request
- Can stream responses
- Tool calls work with generated UUIDs
- Both Gemini 2.5 and 3 thinking work
