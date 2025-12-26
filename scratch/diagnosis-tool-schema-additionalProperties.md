# Diagnosis: Tool Schema Missing additionalProperties

## Error

```
invalid_request_error (invalid_function_parameters): Invalid schema for function 'glob':
In context=(), 'additionalProperties' is required to be supplied and to be false
```

Occurs with all GPT models when using the new provider abstraction.

## Root Cause

**Updated 2024-12-25:** Original diagnosis pointed to wrong file.

The OLD code path (`src/tool.c:ik_tool_build_schema_from_def()`) already has `additionalProperties: false` at line 197.

The actual issue is in the NEW provider abstraction:

`src/providers/request.c:328-400` - `build_tool_parameters_json()`

This function builds the tool parameter schema for the canonical request format but does NOT add `additionalProperties: false`:

```c
// Current code (lines 337-385):
yyjson_mut_val *parameters = yyjson_mut_obj(doc);
// ...
if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) { ... }
if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) { ... }
if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) { ... }
// MISSING: additionalProperties: false
```

### Why This Matters

`src/providers/openai/request_chat.c:serialize_chat_tool()` (line 65) sets `strict: true` on all tools:

```c
// Add strict: true for structured outputs
if (!yyjson_mut_obj_add_bool(doc, func_obj, "strict", true)) { ... }
```

OpenAI's strict mode requires `additionalProperties: false` in the schema.

## Call Chain

```
ik_request_build_from_conversation()           [src/providers/request.c:478]
  └─> build_tool_parameters_json()             [src/providers/request.c:328]  ← MISSING additionalProperties
        └─> returns JSON string like: {"type":"object","properties":{...},"required":[...]}

ik_openai_serialize_chat_request()             [src/providers/openai/request_chat.c:117]
  └─> serialize_chat_tool()                    [src/providers/openai/request_chat.c:22]
        └─> sets strict: true                  [line 65]  ← REQUIRES additionalProperties: false
```

## Fix

`src/providers/request.c:385` - Add after the `required` array:

```c
if (!yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)) {  // LCOV_EXCL_BR_LINE
    PANIC("Failed to add additionalProperties field");  // LCOV_EXCL_LINE
}
```

## Files

| File | Line | Change |
|------|------|--------|
| src/providers/request.c | 385 | Add `yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)` |

## Related Issues (Same Session)

### Claude "no tools" message

**NOT A BUG.** Claude receives tools correctly (`src/providers/anthropic/request.c:serialize_tools()` works). Claude's response "I can answer without tools" for "2+2" is intelligent - it doesn't need bash/grep to do math. Verify with tool-requiring prompt like "list files in src/".

### Gemini 404

Likely model name mismatch. `gemini-3-flash` may not exist. Current models:
- `gemini-2.0-flash`
- `gemini-2.5-flash`
- `gemini-2.5-pro`

Base URL: `https://generativelanguage.googleapis.com/v1beta` (may need `v1` for some models).
