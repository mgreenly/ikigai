# rel-04: Local Tool Execution

## Objective

Enable file operations, shell commands, and search capabilities through LLM tool use.

## Design Decisions

### Internal Representation

Design internal structures that support multiple LLM providers:
- OpenAI (implementing now)
- Anthropic (future)
- Google (future)
- Meta (future)
- X.AI (future)

For rel-04, only OpenAI API is implemented, but internal structures are provider-agnostic.

### Canonical Message Format

The in-memory conversation uses a **canonical format** that is provider-agnostic. This format closely mirrors the database storage format, using a `kind` discriminator:

```c
typedef struct {
    char *kind;        // "system", "user", "assistant", "tool_call", "tool_result"
    char *content;     // Human-readable text (NULL for tool_call)
    char *data_json;   // Structured data (NULL for text-only messages)
} ik_msg_t;
```

**Message kinds and their data:**

| kind | content | data_json |
|------|---------|-----------|
| `system` | System prompt text | NULL |
| `user` | User input text | NULL |
| `assistant` | Response text | NULL |
| `tool_call` | Human-readable summary | `{id, type, function: {name, arguments}}` |
| `tool_result` | Human-readable summary | `{tool_call_id, output, success}` |

**Why canonical format?**

1. **Provider independence**: Each AI provider has different wire formats
   - OpenAI: `role: "assistant"` with `tool_calls` array
   - Anthropic: `role: "assistant"` with `tool_use` content blocks
   - Future providers may differ further

2. **Single source of truth**: Memory and database use the same structure

3. **Clean serialization boundary**: Provider modules convert canonical → wire format

**Conversation in memory:**

```c
typedef struct {
    ik_msg_t **messages;
    size_t count;
} ik_conversation_t;
```

The `repl->conversation` holds canonical messages. When making an API request, the provider-specific serializer (e.g., `ik_openai_serialize_request()`) transforms canonical messages to the wire format.

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

**Independence**: These two events are completely independent - no transactional coupling. Each event is persisted separately. If one fails, it does not affect the other. Memory is authoritative during a session; the database is an event log for session restore. Orphaned events (e.g., tool_call without tool_result due to crash) are rare edge cases that the model can recover from gracefully.

### Message Transformation (Canonical to Wire Format)

The canonical message format is provider-agnostic. Each provider module transforms canonical messages to its wire format during API request serialization.

#### Canonical Format (In-Memory and Database)

Messages use a `kind` discriminator with optional `data_json`:

```
{kind: "tool_call", content: "glob(...)", data_json: {id, type, function: {name, arguments}}}
{kind: "tool_result", content: "3 files", data_json: {tool_call_id, output, success}}
```

#### Provider-Specific Wire Formats

**OpenAI** transforms canonical messages as follows:

| Canonical kind | OpenAI wire format |
|----------------|-------------------|
| `system` | `{role: "system", content: "..."}` |
| `user` | `{role: "user", content: "..."}` |
| `assistant` | `{role: "assistant", content: "..."}` |
| `tool_call` | `{role: "assistant", tool_calls: [...]}` |
| `tool_result` | `{role: "tool", tool_call_id: "...", content: "..."}` |

**Anthropic** (future) would transform differently:

| Canonical kind | Anthropic wire format |
|----------------|----------------------|
| `system` | Separate `system` parameter |
| `user` | `{role: "user", content: [...]}` |
| `assistant` | `{role: "assistant", content: [...]}` |
| `tool_call` | `{role: "assistant", content: [{type: "tool_use", ...}]}` |
| `tool_result` | `{role: "user", content: [{type: "tool_result", ...}]}` |

#### Serialization Boundary

Transformation happens in provider-specific serializers:

```c
// OpenAI serializer transforms canonical → OpenAI JSON
char *ik_openai_serialize_request(void *parent, ik_conversation_t *conv, ...);

// Future: Anthropic serializer transforms canonical → Anthropic JSON
char *ik_anthropic_serialize_request(void *parent, ik_conversation_t *conv, ...);
```

This design ensures:
1. **Single conversation format** in memory and database
2. **Provider isolation** - wire format details contained in provider modules
3. **Easy multi-provider support** - add new serializer, no core changes

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
