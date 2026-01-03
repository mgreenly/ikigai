# Helpful Error Messages from Tools

The tool wrapper is responsible for providing **actionable, helpful error messages**. The LLM simply relays them to the user.

## Design Principle

**Tool errors should be self-explanatory and include:**
- What went wrong (clear error description)
- Why it matters (context)
- How to fix it (step-by-step instructions)
- Where to get help (URLs, docs, examples)

**LLM's job:** Pass the error message through, maybe add context about what the user was trying to do.

## Example Error Scenarios

### 1. Missing Credentials

**User asks:** "Query the customer database for recent signups"

**LLM calls:**
```json
{"tool": "db_query", "query": "recent signups"}
```

**Tool returns:**
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

**LLM responds to user:**
```
I tried to query the database but it isn't configured yet.

To use the database tool, you need connection credentials:

1. Get your database credentials from the internal wiki
2. Set environment variable: export DB_CONNECTION_STRING=your-connection
3. Or create file: ~/.config/db-query/credentials

After setup, database queries will work automatically. Just ask me again!
```

**Notice:** LLM just reformatted the error. The **tool provided all the intelligence**.

### 2. Invalid Credentials

**After user sets wrong credentials:**

**Tool returns:**
```json
{
  "error": "Database authentication failed",
  "error_code": "INVALID_CREDENTIALS",
  "help": {
    "message": "Your credentials were rejected",
    "steps": [
      "1. Verify your credentials on the internal wiki",
      "2. Check if your account has database access",
      "3. Ensure the connection string format is correct",
      "4. Update your configuration and try again"
    ]
  }
}
```

**LLM responds:**
```
The database connection failed - your credentials were rejected.

Your credentials were rejected. Here's how to fix it:

1. Verify your credentials on the internal wiki
2. Check if your account has database access
3. Ensure the connection string format is correct
4. Update your configuration and try again

Make sure you copied the entire connection string without extra spaces.
```

### 3. Rate Limit Exceeded

**Tool returns:**
```json
{
  "error": "API rate limit exceeded",
  "error_code": "RATE_LIMIT",
  "help": {
    "message": "You've exceeded the query limit for this hour",
    "steps": [
      "Wait 60 minutes for the limit to reset",
      "Or contact the infrastructure team for higher limits"
    ]
  }
}
```

**LLM responds:**
```
I can't query right now - you've hit the rate limit.

You've exceeded the query limit for this hour. You can either:
- Wait 60 minutes for the limit to reset
- Or contact the infrastructure team for higher limits

Is there anything else I can help with in the meantime?
```

### 4. Invalid Parameter

**User asks:** "Show me 1000 results from the database"

**LLM calls:**
```json
{"tool": "db_query", "query": "all users", "limit": 1000}
```

**Tool returns:**
```json
{
  "error": "Parameter 'limit' must be between 1 and 100",
  "error_code": "INVALID_PARAM",
  "help": "You requested 1000 results, but valid range is 1-100"
}
```

**LLM responds:**
```
I can't request 1000 results - the tool only allows 1-100 results per query.

Let me query the top 100 results instead:
[Makes new call with limit: 100]
```

**Notice:** LLM understands the constraint and adapts automatically.

### 5. Network Error

**Tool returns:**
```json
{
  "error": "Network error connecting to database: Connection timeout",
  "error_code": "NETWORK_ERROR",
  "help": "Check that the database server is accessible from your network"
}
```

**LLM responds:**
```
I couldn't connect to the database - the request timed out.

This might be a temporary network issue. Check that the database server
is accessible from your network, and we can try again in a moment.
```

## Error Message Structure

### Minimum Required

```json
{
  "error": "Brief description of what went wrong",
  "error_code": "MACHINE_READABLE_CODE"
}
```

### Helpful Addition

```json
{
  "error": "Brief description",
  "error_code": "MACHINE_READABLE_CODE",
  "help": "Single line guidance"
}
```

### Maximum Helpfulness

```json
{
  "error": "Brief description",
  "error_code": "MACHINE_READABLE_CODE",
  "help": {
    "message": "Context about the error",
    "steps": [
      "1. First thing to try",
      "2. Second thing to try",
      "3. Where to get more help"
    ],
    "note": "Additional context or reassurance"
  }
}
```

## Standard Error Codes

Tools should use consistent error codes:

- `MISSING_CREDENTIALS` - API key / auth not configured
- `INVALID_CREDENTIALS` - API key / auth rejected
- `RATE_LIMIT` - Quota exceeded
- `INVALID_PARAM` - Parameter validation failed
- `MISSING_PARAM` - Required parameter not provided
- `NETWORK_ERROR` - Network/connectivity issue
- `TIMEOUT` - Request timed out
- `API_ERROR` - External API returned error
- `INTERNAL_ERROR` - Tool implementation error

## Best Practices

### DO: Be Specific
```json
{
  "error": "Database connection not configured",
  "help": "Set DB_CONNECTION_STRING environment variable or create ~/.config/db-query/credentials"
}
```

### DON'T: Be Vague
```json
{
  "error": "Configuration error"
}
```

### DO: Provide Steps
```json
{
  "help": {
    "steps": [
      "1. Get credentials from internal wiki",
      "2. Run: export DB_CONNECTION_STRING=your-connection",
      "3. Try the query again"
    ]
  }
}
```

### DON'T: Just Point to Docs
```json
{
  "help": "See documentation at https://example.com/docs"
}
```

### DO: Include Context
```json
{
  "error": "Rate limit exceeded",
  "help": "You've made 100/100 queries this hour. Resets at 3:00 PM."
}
```

### DON'T: Just State the Error
```json
{
  "error": "HTTP 429"
}
```

## Benefits

**For Users:**
- Self-service problem resolution
- Clear action items
- No guessing what went wrong

**For LLMs:**
- Can relay helpful information without understanding the domain
- Error codes enable smart retry logic
- Structured help enables better user guidance

**For Tool Developers:**
- Errors are first-class outputs
- Testing includes error scenarios
- Documentation lives in the code

**For ikigai:**
- No error interpretation needed
- No credential management
- Just passes messages through
