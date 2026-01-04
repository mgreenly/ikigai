# Tool Schema Format Gap

The current internal schema format defined in `cdd/plan/tool-specifications.md` cannot express features that all three major providers support.

## Current Format

```json
{
  "name": "tool_name",
  "description": "...",
  "parameters": {
    "param1": {
      "type": "string",
      "description": "...",
      "required": true
    }
  },
  "returns": {...}
}
```

## Provider Comparison

All three providers use JSON Schema (or OpenAPI schema, which is JSON Schema-based):

| Feature | OpenAI | Anthropic | Google | ikigai |
|---------|--------|-----------|--------|--------|
| string, integer, boolean | Y | Y | Y | Y |
| number (float) | Y | Y | Y | N |
| array | Y | Y | Y | N |
| enum | Y | Y | Y | N |
| nested objects | Y | Y | Y | N |
| nullable | Y | Y | Y | N |
| format (date, uri, etc.) | Y | Y | Y | N |
| items (array schema) | Y | Y | Y | N |
| required as array | Y | Y | Y | N |

## Decision

Adopt JSON Schema as ikigai's internal format.

**Rationale:**
- All providers already use JSON Schema
- Translation to provider formats becomes trivial (subset extraction, not invention)
- User-defined tools can express full capabilities
- No need to invent and maintain a custom schema language

## Impact on Plan

The `--schema` output format in `cdd/plan/tool-specifications.md` needs revision:

**Before (custom format):**
```json
{
  "name": "file_read",
  "parameters": {
    "file_path": {
      "type": "string",
      "required": true
    }
  }
}
```

**After (JSON Schema):**
```json
{
  "name": "file_read",
  "description": "Read contents of a file",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Absolute or relative path to file"
      }
    },
    "required": ["file_path"]
  }
}
```

## Files to Update

- `cdd/plan/tool-specifications.md` - All 6 tool schemas
- `cdd/plan/tool-discovery-execution.md` - Schema protocol section
- `cdd/plan/architecture.md` - If it references schema format

## Sources

- [OpenAI Function Calling](https://platform.openai.com/docs/guides/function-calling)
- [Anthropic Tool Use](https://docs.anthropic.com/en/docs/agents-and-tools/tool-use/implement-tool-use)
- [Google Gemini Function Calling](https://ai.google.dev/gemini-api/docs/function-calling)
