# Error Codes

Standard error codes for tool responses.

## ikigai Error Codes

Used when `tool_success: false` (ikigai-level failures):

| Code | Meaning |
|------|---------|
| `TOOL_TIMEOUT` | Tool exceeded execution timeout |
| `TOOL_CRASHED` | Tool exited with non-zero code |
| `INVALID_OUTPUT` | Tool returned malformed JSON |
| `TOOL_NOT_FOUND` | Tool not in registry |
| `INVALID_PARAMS` | ikigai failed to marshal parameters (should not happen) |

## Tool Error Codes

Used in tool responses when `tool_success: true` but operation failed:

| Code | Meaning |
|------|---------|
| `MISSING_CREDENTIALS` | API key / auth not configured |
| `INVALID_CREDENTIALS` | API key / auth rejected |
| `RATE_LIMIT` | Quota exceeded |
| `INVALID_PARAM` | Parameter validation failed |
| `MISSING_PARAM` | Required parameter not provided |
| `NETWORK_ERROR` | Network/connectivity issue |
| `TIMEOUT` | Request timed out |
| `API_ERROR` | External API returned error |
| `INTERNAL_ERROR` | Tool implementation error |

## Error Response Structure

Minimum:
```json
{
  "error": "Brief description",
  "error_code": "CODE"
}
```

With help:
```json
{
  "error": "Brief description",
  "error_code": "CODE",
  "help": {
    "message": "Context",
    "steps": ["1. First step", "2. Second step"],
    "note": "Additional info"
  }
}
```
