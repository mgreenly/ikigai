# Tool Missing Credentials

Tool guides user through credential setup when first used.

## Description

User tries to use a custom tool that requires credentials. The tool returns a helpful error with setup instructions. LLM relays this to user. After setup, tool works immediately.

## Transcript

```text
> query our customer database for recent signups

I'll query the database for that.

The database tool isn't configured yet. Here's how to set it up:

1. Get your database credentials from the internal wiki
2. Set environment variable: export DB_CONNECTION_STRING=your-connection
3. Or create file: ~/.config/db-query/credentials

After setup, just ask me again and it'll work.

> [user sets DB_CONNECTION_STRING in their shell, restarts ikigai]

> query our customer database for recent signups

Here's what I found:

| Name | Signup Date |
|------|-------------|
| Alice | 2025-01-15 |
| Bob | 2025-01-14 |
...
```

## Walkthrough

1. User asks LLM to query database
2. LLM calls `db_query` tool (see [Tool Call](#tool-call))
3. ikigai executes tool
4. Tool checks for credentials, finds none
5. Tool returns error with help (see [Tool Response](#tool-response))
6. ikigai wraps response (see [Wrapped Response](#wrapped-response))
7. LLM receives structured error, formats help for user
8. User configures credentials
9. User retries, tool works

## Reference

### Tool Call

```json
{
  "tool_calls": [
    {
      "id": "call_xyz789",
      "type": "function",
      "function": {
        "name": "db_query",
        "arguments": "{\"query\": \"recent signups\"}"
      }
    }
  ]
}
```

### Tool Response

```json
{
  "error": "Database connection not configured",
  "error_code": "MISSING_CREDENTIALS",
  "help": {
    "message": "To use the database tool, you need connection credentials",
    "steps": [
      "1. Get your database credentials from the internal wiki",
      "2. Set environment variable: export DB_CONNECTION_STRING=your-connection",
      "3. Or create file: ~/.config/db-query/credentials"
    ],
    "note": "After setup, this tool will work automatically"
  }
}
```

### Wrapped Response

```json
{
  "tool_success": true,
  "result": {
    "error": "Database connection not configured",
    "error_code": "MISSING_CREDENTIALS",
    "help": {
      "message": "To use the database tool, you need connection credentials",
      "steps": [
        "1. Get your database credentials from the internal wiki",
        "2. Set environment variable: export DB_CONNECTION_STRING=your-connection",
        "3. Or create file: ~/.config/db-query/credentials"
      ],
      "note": "After setup, this tool will work automatically"
    }
  }
}
```

## Notes

- `tool_success: true` because the tool executed correctly
- The error is a tool-level error (missing config), not an ikigai error
- LLM is expected to format the help steps for the user
- No restart needed after setting env var (tool checks each execution)
- This pattern applies to any tool requiring credentials (API keys, database connections, etc.)
