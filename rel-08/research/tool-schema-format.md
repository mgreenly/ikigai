# Tool Schema Format

## Overview

External tools describe themselves via `--schema` flag. This document defines exactly what that output must contain.

## Format: JSON Schema

Tools return JSON Schema (Draft 2020-12 compatible subset) for parameter definitions. This is the native format for all three LLM providers.

## Complete Schema Structure

```json
{
  "name": "tool_name",
  "description": "What the tool does",
  "parameters": {
    "type": "object",
    "properties": {
      "param1": {
        "type": "string",
        "description": "Parameter description"
      }
    },
    "required": ["param1"]
  }
}
```

## Field Definitions

### Top-Level Fields

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `name` | Yes | string | Tool identifier (alphanumeric + underscore) |
| `description` | Yes | string | What the tool does (shown to LLM) |
| `parameters` | Yes | object | JSON Schema for input parameters |

### Parameters Object

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `type` | Yes | "object" | Always "object" for tool parameters |
| `properties` | Yes | object | Map of parameter name to schema |
| `required` | No | array | Parameter names that must be provided |

### Property Schema

Each property in `properties` is a JSON Schema:

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `type` | Yes | string | One of: string, integer, number, boolean, array, object |
| `description` | Recommended | string | Parameter description (shown to LLM) |
| `enum` | No | array | Restrict to specific values |
| `items` | For arrays | object | Schema for array elements |
| `properties` | For objects | object | Nested property schemas |
| `required` | For objects | array | Required nested properties |

## Concrete Example: file_edit

```json
{
  "name": "file_edit",
  "description": "Edit a file by replacing exact text matches. You must read the file before editing.",
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
        "description": "Replace all occurrences (default: false, fails if not unique)"
      }
    },
    "required": ["file_path", "old_string", "new_string"]
  }
}
```

## Supported Types

### Primitive Types

```json
{"type": "string", "description": "Text value"}
{"type": "integer", "description": "Whole number"}
{"type": "number", "description": "Decimal number"}
{"type": "boolean", "description": "True or false"}
```

### Enum (Restricted Values)

```json
{
  "type": "string",
  "description": "Output format",
  "enum": ["json", "text", "markdown"]
}
```

### Array

```json
{
  "type": "array",
  "description": "List of file paths",
  "items": {
    "type": "string"
  }
}
```

### Nested Object

```json
{
  "type": "object",
  "description": "Filter options",
  "properties": {
    "include": {"type": "string"},
    "exclude": {"type": "string"}
  },
  "required": ["include"]
}
```

## What We Do NOT Support

These JSON Schema features are not used:

- `$ref` (schema references)
- `$defs` (schema definitions)
- `allOf`, `anyOf`, `oneOf` (composition)
- `if`, `then`, `else` (conditionals)
- `pattern` (regex validation)
- `format` (semantic formats like "email", "uri")
- `default` (default values)
- `additionalProperties` (ikigai adds this during translation)

## Name Conventions

- Tool name: `snake_case` (e.g., `file_edit`, `file_read`)
- Binary name: `kebab-case` (e.g., `file-edit`, `file-read`)
- Parameter names: `snake_case` (e.g., `file_path`, `old_string`)

## Validation Rules

ikigai validates schemas at discovery time:

1. **Valid JSON** - Must parse successfully
2. **Required fields** - `name`, `description`, `parameters` present
3. **Name format** - Alphanumeric and underscore only
4. **Parameters structure** - `type: "object"`, `properties` present
5. **Property types** - Each property has valid `type`
6. **Required consistency** - Required array only lists defined properties

## Error Handling

If `--schema` returns invalid output:
- Tool is not registered
- Error logged with tool path and reason
- Other tools continue loading
