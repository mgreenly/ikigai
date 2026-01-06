# Provider Schema Translation

## Overview

Each LLM provider has slightly different tool definition formats. This document shows exactly how ikigai translates the canonical tool schema to each provider's API.

## Canonical Format (from --schema)

```json
{
  "name": "file_edit",
  "description": "Edit a file by replacing exact text matches.",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      },
      "old_string": {
        "type": "string",
        "description": "Exact text to find and replace"
      },
      "new_string": {
        "type": "string",
        "description": "Text to replace old_string with"
      },
      "replace_all": {
        "type": "boolean",
        "description": "Replace all occurrences (default: false)"
      }
    },
    "required": ["file_path", "old_string", "new_string"]
  }
}
```

## OpenAI Translation

OpenAI Chat Completions API wraps tools in a `function` object and requires `strict: true` mode.

### OpenAI Format

```json
{
  "type": "function",
  "function": {
    "name": "file_edit",
    "description": "Edit a file by replacing exact text matches.",
    "strict": true,
    "parameters": {
      "type": "object",
      "properties": {
        "file_path": {
          "type": "string",
          "description": "Absolute or relative path to file"
        },
        "old_string": {
          "type": "string",
          "description": "Exact text to find and replace"
        },
        "new_string": {
          "type": "string",
          "description": "Text to replace old_string with"
        },
        "replace_all": {
          "type": "boolean",
          "description": "Replace all occurrences (default: false)"
        }
      },
      "required": ["file_path", "old_string", "new_string", "replace_all"],
      "additionalProperties": false
    }
  }
}
```

### OpenAI Translation Rules

| Action | Reason |
|--------|--------|
| Wrap in `{"type": "function", "function": {...}}` | OpenAI format |
| Add `strict: true` | Enable structured outputs |
| Add `additionalProperties: false` | Required for strict mode |
| Add ALL properties to `required` | Required for strict mode |

**Note:** OpenAI strict mode requires every property to be in `required[]`, even optional ones. This is a quirk of their structured outputs feature.

## Anthropic Translation

Anthropic uses `input_schema` instead of `parameters`.

### Anthropic Format

```json
{
  "name": "file_edit",
  "description": "Edit a file by replacing exact text matches.",
  "input_schema": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      },
      "old_string": {
        "type": "string",
        "description": "Exact text to find and replace"
      },
      "new_string": {
        "type": "string",
        "description": "Text to replace old_string with"
      },
      "replace_all": {
        "type": "boolean",
        "description": "Replace all occurrences (default: false)"
      }
    },
    "required": ["file_path", "old_string", "new_string"]
  }
}
```

### Anthropic Translation Rules

| Action | Reason |
|--------|--------|
| Rename `parameters` to `input_schema` | Anthropic naming |
| Keep `required` as-is | Anthropic respects optional params |

**Note:** Anthropic's format is closest to our canonical format.

## Google Gemini Translation

Google wraps tools in `functionDeclarations` and doesn't support `additionalProperties`.

### Google Format

```json
{
  "tools": [{
    "functionDeclarations": [{
      "name": "file_edit",
      "description": "Edit a file by replacing exact text matches.",
      "parameters": {
        "type": "object",
        "properties": {
          "file_path": {
            "type": "string",
            "description": "Absolute or relative path to file"
          },
          "old_string": {
            "type": "string",
            "description": "Exact text to find and replace"
          },
          "new_string": {
            "type": "string",
            "description": "Text to replace old_string with"
          },
          "replace_all": {
            "type": "boolean",
            "description": "Replace all occurrences (default: false)"
          }
        },
        "required": ["file_path", "old_string", "new_string"]
      }
    }]
  }]
}
```

### Google Translation Rules

| Action | Reason |
|--------|--------|
| Wrap in `tools[].functionDeclarations[]` | Google structure |
| Remove `additionalProperties` | Google doesn't support it |
| Keep `required` as-is | Google respects optional params |

## Translation Summary

| Field | OpenAI | Anthropic | Google |
|-------|--------|-----------|--------|
| Wrapper | `{type: "function", function: {...}}` | none | `functionDeclarations[]` |
| Schema key | `parameters` | `input_schema` | `parameters` |
| `additionalProperties` | Add `false` | Passthrough | Remove |
| `required` array | Add ALL properties | Keep as-is | Keep as-is |
| `strict` | Add `true` | N/A | N/A |

## Validation Steps

### 1. At Discovery Time (tool --schema)

| Check | Action on Failure |
|-------|-------------------|
| Valid JSON | Skip tool, log error |
| Has `name` string | Skip tool, log error |
| Has `description` string | Skip tool, log error |
| Has `parameters` object | Skip tool, log error |
| `parameters.type` is "object" | Skip tool, log error |
| `parameters.properties` exists | Skip tool, log error |
| Each property has valid `type` | Skip tool, log error |

### 2. At Registration Time

| Check | Action on Failure |
|-------|-------------------|
| Name is unique | Skip tool, log warning |
| Name format valid (alphanumeric + underscore) | Skip tool, log error |

### 3. At Request Time (Provider Serialization)

| Check | Action on Failure |
|-------|-------------------|
| Schema parses as JSON | PANIC (should never happen - validated at discovery) |

### 4. Never (ikigai does NOT validate)

| What | Why Not |
|------|---------|
| Tool input matches schema | Provider validates this |
| Tool output matches return schema | Not critical for operation |

## Implementation Notes

### Current Code Location

Provider-specific translation happens in:
- `src/providers/openai/request_chat.c` - OpenAI serialization
- `src/providers/anthropic/request.c` - Anthropic serialization
- `src/providers/google/request.c` - Google serialization

### Shared Tool Definition

Tools are registered with the canonical format. Each provider serializer transforms during request building:

```c
// Current internal representation
typedef struct {
    const char *name;
    const char *description;
    const char *parameters;  // JSON string of parameters schema
    bool strict;
} ik_tool_def_t;
```

The `parameters` field stores the JSON Schema string. Each provider's request serializer parses this and transforms as needed.

## Sources

- [OpenAI Function Calling](https://platform.openai.com/docs/guides/function-calling)
- [OpenAI Structured Outputs](https://openai.com/index/introducing-structured-outputs-in-the-api/)
- [Anthropic Tool Use](https://docs.anthropic.com/en/docs/agents-and-tools/tool-use/implement-tool-use)
- [Google Gemini Function Calling](https://ai.google.dev/gemini-api/docs/function-calling)
