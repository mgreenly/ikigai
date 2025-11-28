# Database Debug Output Specification

## Overview

When `debug on` is enabled, the database layer outputs SQL queries, parameters, and results to the `[db]` debug pipe. This provides visibility into database operations for troubleshooting and development.

## Architecture

The database debug output follows the same pattern as OpenAI debug output:
- Debug output is written to `repl->db_debug_pipe->write_end` (a FILE*)
- Output is only generated when `repl->debug_enabled` is true
- All output is prefixed with `[db]` by the debug pipe system
- Output is line-buffered and displayed in the scrollback when debug is enabled

## Output Format

### Connection Operations (Task 2: libpq-integration)

**Connection attempt:**
```
[db] Connecting to: postgresql://user@localhost:5432/ikigai
[db] Connection status: SUCCESS
```

**Connection failure:**
```
[db] Connecting to: postgresql://user@localhost:5432/ikigai_nonexistent
[db] Connection status: FAILED - database "ikigai_nonexistent" does not exist
```

**Security:** Connection strings with passwords are redacted:
```
postgresql://user:password@host/db → postgresql://user:***@host/db
```

### Session Operations (Task 3: session-lifecycle)

**Session creation:**
```
[db] SQL: INSERT INTO sessions DEFAULT VALUES RETURNING id
[db] Result: session_id=42
```

**Session end:**
```
[db] SQL: UPDATE sessions SET ended_at = NOW() WHERE id = $1
[db] Params: session_id=42
[db] Result: 1 row updated
```

**Error example:**
```
[db] SQL: INSERT INTO sessions DEFAULT VALUES RETURNING id
[db] Error: connection to server was lost
```

### Message Persistence (Task 4: message-persistence)

**Message insert:**
```
[db] SQL: INSERT INTO messages (session_id, kind, content, data) VALUES ($1, $2, $3, $4)
[db] Params: session_id=42, kind=user, content_length=156, data={"model":"claude-sonnet-4"}
[db] Result: message_id=127
```

**Different event kinds:**
```
[db] Params: session_id=42, kind=clear, content_length=0, data=null
[db] Params: session_id=42, kind=system, content_length=89, data=null
[db] Params: session_id=42, kind=assistant, content_length=523, data={"tokens":245}
[db] Params: session_id=42, kind=mark, content_length=0, data={"label":"checkpoint1"}
[db] Params: session_id=42, kind=rewind, content_length=0, data={"target_mark_id":98}
```

### Replay Algorithm (Task 5: replay-algorithm)

**Loading messages:**
```
[db] SQL: SELECT id, kind, content, data FROM messages WHERE session_id=$1 ORDER BY created_at
[db] Params: session_id=42
[db] Result: 15 rows returned
```

**Replay processing:**
```
[db] Replay: processing event #1 kind=clear
[db] Replay: processing event #2 kind=system
[db] Replay: processing event #3 kind=user
[db] Replay: processing event #4 kind=assistant
[db] Replay: processing event #5 kind=mark label=checkpoint1
[db] Replay: processing event #6 kind=user
[db] Replay: processing event #7 kind=rewind to_mark=checkpoint1
[db] Replay: final context has 5 messages (2 cleared by rewind)
```

## Implementation Guidelines

### When to output

Debug output should be generated:
1. **Before SQL execution** - show the query and parameters
2. **After successful execution** - show the result summary
3. **On error** - show the error message

### How to output

```c
// Check if debug is enabled
if (repl->debug_enabled && repl->db_debug_pipe) {
    FILE *debug = repl->db_debug_pipe->write_end;

    // Output query
    fprintf(debug, "[db] SQL: %s\n", sql_query);

    // Output parameters
    fprintf(debug, "[db] Params: session_id=%ld, kind=%s\n", session_id, kind);

    // Output result
    fprintf(debug, "[db] Result: message_id=%ld\n", message_id);

    // Always flush to ensure immediate visibility
    fflush(debug);
}
```

### Error handling

Always output errors even if query fails:
```c
if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    if (repl->debug_enabled && repl->db_debug_pipe) {
        fprintf(repl->db_debug_pipe->write_end, "[db] Error: %s\n",
                PQerrorMessage(conn));
        fflush(repl->db_debug_pipe->write_end);
    }
    // ... handle error
}
```

### Testing

Each task's test suite should include tests for debug output:
- Test that output is generated when `debug_enabled=true`
- Test that no output is generated when `debug_enabled=false`
- Test that output format matches specification
- Test that sensitive information (passwords) is redacted

## User Experience

When a user enables debug mode and performs database operations:

```
> debug on
Debug mode enabled

> /clear
[db] SQL: INSERT INTO messages (session_id, kind, content, data) VALUES ($1, $2, $3, $4)
[db] Params: session_id=42, kind=clear, content_length=0, data=null
[db] Result: message_id=128

> hello
[db] SQL: INSERT INTO messages (session_id, kind, content, data) VALUES ($1, $2, $3, $4)
[db] Params: session_id=42, kind=user, content_length=5, data={"model":"claude-sonnet-4"}
[db] Result: message_id=129
[openai] * Connected to api.anthropic.com
[openai] > POST /v1/messages HTTP/2
...
[db] SQL: INSERT INTO messages (session_id, kind, content, data) VALUES ($1, $2, $3, $4)
[db] Params: session_id=42, kind=assistant, content_length=234, data={"tokens":89}
[db] Result: message_id=130

Hello! How can I help you today?
```

## Benefits

1. **Transparency** - See exactly what SQL is being executed
2. **Performance monitoring** - Observe query patterns and frequency
3. **Debugging** - Diagnose database-related issues quickly
4. **Learning** - Understand how the event stream model works
5. **Audit** - Track all database operations during a session

## Related Documentation

- `.tasks/v0.3.0-database/` - Database integration task series
- `docs/debug-pipe.md` - Debug pipe architecture (if exists)
- `src/debug_pipe.h` - Debug pipe API documentation
