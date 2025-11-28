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

### Display

Show full transparency in scrollback:
- Full tool call with parameter values
- Full result output from tool execution

### Execution Model

- Sequential execution (one tool at a time)
- No parallel tool execution in this release
- Bash execution is blocking

### Loop Limits

Configuration setting `max_tool_turns` with default of 50.

Prevents runaway tool loops while allowing substantial autonomous work.

### Error Handling

Return error text to LLM as tool result, let it decide recovery strategy.

Example:
```
Error: File not found: /path/to/missing.txt
```

Simple text errors - no complex JSON structures for tool results.

### Security Model

Full trust model for rel-04:
- No guardrails or prevention
- User sees exactly what tool calls are executed
- User's machine, user's responsibility

Guardrails may be discussed for future releases.

### Static Tool List

Tools are statically defined and always available. Dynamic tool configuration deferred to future release.

### Out of Scope

- Tree-sitter code analysis (future release)
- Parallel tool execution
- Tool confirmation prompts
- Guardrails/sandboxing
- Multi-provider support (designed for, not implemented)

## Testing Strategy

Real filesystem operations in test fixtures.

## Configuration

New config fields:
- `max_tool_turns` (int, default: 50) - Maximum tool call iterations per turn
