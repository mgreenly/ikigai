# Diagnosis: Tool Schema Missing additionalProperties

## Error

```
invalid_request_error (invalid_function_parameters): Invalid schema for function 'glob':
In context=(), 'additionalProperties' is required to be supplied and to be false
```

## Root Cause

`src/tool.c:192-196`

The `ik_tool_build_schema_from_def()` function builds the tool schema but does NOT include `"additionalProperties": false` in the parameters object.

```c
// Current code (lines 192-196):
yyjson_mut_val *parameters = yyjson_mut_obj(doc);
if (parameters == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed");
if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed");
// MISSING: additionalProperties: false
```

## Fix

Add after line 196:
```c
if (!yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
```

This affects ALL tools (glob, file_read, grep, file_write, bash) since they all use `ik_tool_build_schema_from_def()`.

## Files

| File | Line | Change |
|------|------|--------|
| src/tool.c | 196 | Add `yyjson_mut_obj_add_bool(doc, parameters, "additionalProperties", false)` |
