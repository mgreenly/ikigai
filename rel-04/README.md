# rel-04: Local Tool Execution

## Objective

Enable file operations, shell commands, and search capabilities through LLM tool use.

## Design Decisions

### Internal Representation

Design an internal `ik_tool_call_t` struct that can support multiple LLM providers in the future:
- OpenAI (implementing now)
- Anthropic (future)
- Google (future)
- Meta (future)
- X.AI (future)

For rel-04, only OpenAI tool calling is implemented, but the internal structures are designed for future multi-provider support.

### Tools

Five tools in implementation order:

1. **glob** - Find files matching pattern
2. **file_read** - Read file contents
3. **grep** - Search file contents
4. **file_write** - Write content to file
5. **bash** - Execute shell command (blocking execution)

### Implementation Phases

1. **First slice**: Complete vertical implementation with `glob` + `file_read`
   - Tool data structures
   - Tool registry
   - Request serialization (add `tools` to OpenAI request)
   - Response parsing (detect tool calls in SSE stream)
   - Tool execution
   - Conversation loop (re-submit with tool results)
   - Database persistence
   - UI feedback in scrollback

2. **Subsequent tools**: Add `grep`, `file_write`, `bash` one at a time after first slice is complete

### Database Persistence

Explicit event model:
- One event for tool request (the LLM's tool_call)
- One event for tool response (the execution result)

This provides a complete audit trail matching exactly what was received and executed.

### Message Transformation (Replay to API)

A critical aspect of the replay system is transforming messages from database storage format to OpenAI API request format.

#### Database Storage Format

Tool messages are stored in the database with specific `kind` values:

- `kind: "tool_call"` - Stores the assistant's request to execute a tool
  - Contains `data_json` field with tool call details (function name, arguments, tool_call_id)
  - Represents what the LLM requested to execute

- `kind: "tool_result"` - Stores the execution result
  - Contains `data_json` field with tool output and tool_call_id reference
  - Represents what the tool execution returned

#### API Request Format

When replaying a session, these database messages must be transformed to match the OpenAI API format:

1. **tool_call → assistant message with tool_calls array**
   ```
   Database: {kind: "tool_call", data_json: {tool_calls: [...]}}

   OpenAI API: {
     role: "assistant",
     tool_calls: [
       {
         id: "call_abc123",
         type: "function",
         function: {name: "glob", arguments: "{\"pattern\":\"*.c\"}"}
       }
     ]
   }
   ```

2. **tool_result → tool message with tool_call_id**
   ```
   Database: {kind: "tool_result", data_json: {tool_call_id: "call_abc123", content: "..."}}

   OpenAI API: {
     role: "tool",
     tool_call_id: "call_abc123",
     content: "{\"output\":\"file1.c\\nfile2.c\",\"count\":2}"
   }
   ```

#### Transformation Logic

The replay system implements this transformation in `src/db/replay.c`:

```
if (kind == "tool_call"):
    Extract tool_calls array from data_json
    Create message with:
        role: "assistant"
        tool_calls: [...array from data_json...]

else if (kind == "tool_result"):
    Extract tool_call_id and content from data_json
    Create message with:
        role: "tool"
        tool_call_id: "..." (from data_json)
        content: "..." (from data_json)
```

This ensures that when a session is restored, the conversation history is correctly reconstructed in the format expected by the OpenAI API, allowing the conversation to continue seamlessly from where it left off.

#### data_json Schema for Tool Messages

The database stores messages with two key fields:
- `content` - Human-readable summary (for display in scrollback)
- `data_json` - Structured data (JSONB field for API reconstruction)

**For kind: "tool_call"**

```json
{
  "id": "call_abc123",           // OpenAI tool call ID
  "type": "function",            // Always "function" for now
  "function": {
    "name": "glob",              // Tool name
    "arguments": "{...}"         // JSON string of arguments
  }
}
```

**For kind: "tool_result"**

```json
{
  "tool_call_id": "call_abc123", // Matches the tool_call id
  "name": "glob",                // Tool name (for display)
  "output": "...",               // Tool output string OR full result JSON
  "success": true                // Whether execution succeeded
}
```

**Example: Complete message storage**

Tool call message:
```
kind: "tool_call"
content: "glob(pattern=\"*.c\", path=\"src/\")"  <- human readable
data_json: {"id": "call_abc123", "type": "function", "function": {"name": "glob", "arguments": "{\"pattern\":\"*.c\",\"path\":\"src/\"}"}}
```

Tool result message:
```
kind: "tool_result"
content: "3 files found"                         <- human readable
data_json: {"tool_call_id": "call_abc123", "name": "glob", "output": "{\"output\":\"file1.c\\nfile2.c\\nfile3.c\",\"count\":3}", "success": true}
```

This separation ensures:
- The `content` field provides immediate human-readable context when viewing the scrollback
- The `data_json` field preserves all structured information needed to reconstruct API requests during replay
- Tool execution results can be displayed simply (via `content`) or processed programmatically (via `data_json`)

### Display

Show full transparency in scrollback:
- Full tool call with parameter values
- Full result output from tool execution

### Execution Model

- Sequential execution (one tool at a time)
- No parallel tool execution in this release
- Bash execution is blocking

### Loop Limits

Configuration setting `max_tool_turns` with production default of 50.

Prevents runaway tool loops while allowing substantial autonomous work.

Note: Testing may use lower values (e.g., 3) to easily verify limit behavior.

### Tool Result Format

All tool results use a consistent JSON envelope:

| Tool | Success Format | Notes |
|------|----------------|-------|
| `glob` | `{"output": "file1\nfile2", "count": N}` | `count` = number of matches |
| `file_read` | `{"output": "contents..."}` | Raw file contents in output |
| `grep` | `{"output": "file:line: match", "count": N}` | `count` = number of matches |
| `file_write` | `{"output": "Wrote N bytes to path", "bytes": N}` | `bytes` = bytes written |
| `bash` | `{"output": "stdout/stderr", "exit_code": N}` | Always includes exit code |
| Error | `{"error": "message"}` | Any tool can return error |

For loop limits, add to the result:
```json
{"output": "...", "count": 1, "limit_reached": true, "limit_message": "Tool call limit reached (3). Stopping tool loop."}
```

### Error Handling

Return errors in JSON format, let LLM decide recovery strategy.

Example:
```json
{"error": "File not found: /path/to/missing.txt"}
```

### Security Model

Full trust model for rel-04:
- No guardrails or prevention
- User sees exactly what tool calls are executed
- User's machine, user's responsibility

Guardrails may be discussed for future releases.

### Static Tool List

Tools are statically defined and always available. Dynamic tool configuration deferred to future release.

### Out of Scope

The following limitations are known and explicitly deferred for rel-04:

1. **Streaming tool argument accumulation** - Tool arguments must arrive in a single SSE chunk. Arguments that stream across multiple chunks are not accumulated. The real OpenAI API can stream large arguments across chunks, but we defer this complexity to a future release.

2. **Parallel tool calls** - OpenAI can return multiple tool_calls in a single response (index: 0, 1, 2...). For rel-04, we only handle sequential single tool calls. Parallel execution is deferred.

3. **Bash command timeout** - Bash execution is blocking with no timeout mechanism. Commands that hang will block indefinitely. (Note: This should be addressed in safety configuration, but the limitation is acknowledged here.)

4. **Guardrails and sandboxing** - Full trust model. No command filtering, path restrictions, or sandboxing. The user sees exactly what tool calls are executed, and it's the user's machine and responsibility.

5. **Multi-provider support** - Only OpenAI API is implemented. Anthropic, Google, and other providers are deferred to future releases (though internal structures are designed to support them).

6. **Tree-sitter code analysis** - Advanced code parsing and analysis capabilities are deferred to a future release.

7. **Tool confirmation prompts** - No interactive confirmation before tool execution. All tool calls are executed automatically.

## Testing Strategy

Real filesystem operations in test fixtures.

## Configuration

New config fields:
- `max_tool_turns` (int, default: 50) - Maximum tool call iterations per turn. Prevents runaway tool loops.
- `max_output_size` (int, default: 1048576) - Maximum output size in bytes (1MB) for all tool results. Output is truncated with indicator if exceeded. Applies uniformly to glob, file_read, grep, and bash output.

Note: These limits protect against accidental runaway operations even in the full-trust model. They provide basic safety boundaries without restricting legitimate use cases.
