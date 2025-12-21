# Task: Update REPL Streaming Callbacks for Provider Abstraction

**Model:** sonnet/thinking
**Depends on:** repl-provider-routing.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load source-code` - Map of REPL implementation
- `/load database` - Message persistence patterns

**Source:**
- `src/repl_callbacks.c` - Streaming callbacks (current implementation)
- `src/repl_event_handlers.c` - Event processing and persistence
- `src/providers/types.h` - Stream event types

**Plan:**
- `scratch/plan/streaming.md` - REPL callback section

## Objective

Update REPL streaming callbacks to handle normalized `ik_stream_event_t` types instead of OpenAI-specific events. This includes accumulating content deltas, handling thinking content, and persisting messages with provider metadata.

## Interface

Functions to update:

| Function | Signature | Changes |
|----------|-----------|---------|
| `ik_repl_stream_callback` | `void (ik_stream_event_t *event, void *ctx)` | Handle normalized events |
| `ik_repl_save_message` | `res_t (ik_repl_t *repl, ...)` | Store provider/model/thinking metadata |

Files to update:

- `src/repl_callbacks.c` - Stream event handling
- `src/repl_event_handlers.c` - Message persistence

## Behaviors

### Stream Callback Handling

Handle normalized `ik_stream_event_t` types:

| Event Type | Handler Action |
|------------|----------------|
| `IK_STREAM_CONTENT_DELTA` | Append text to scrollback, update display |
| `IK_STREAM_TOOL_CALL_DELTA` | Accumulate tool call (id, name, arguments) |
| `IK_STREAM_THINKING_DELTA` | Store thinking content separately |
| `IK_STREAM_DONE` | Finalize message, save to database |
| `IK_STREAM_ERROR` | Display error to user, abort request |

**Content Delta Processing:**
```c
case IK_STREAM_CONTENT_DELTA:
    // Append delta text to accumulated content
    ik_scrollback_append(repl->scrollback, event->delta.text);
    // Trigger UI refresh
    ik_repl_request_render(repl);
    break;
```

**Tool Call Accumulation:**
```c
case IK_STREAM_TOOL_CALL_DELTA:
    // Find or create tool call by index
    ik_tool_call_t *tc = find_or_create_tool_call(repl, event->tool_call.index);
    // Append to appropriate field
    if (event->tool_call.id) append_id(tc, event->tool_call.id);
    if (event->tool_call.name) append_name(tc, event->tool_call.name);
    if (event->tool_call.arguments) append_args(tc, event->tool_call.arguments);
    break;
```

**Thinking Content:**
```c
case IK_STREAM_THINKING_DELTA:
    // Accumulate thinking separately (not shown to user during stream)
    append_thinking(repl->pending_thinking, event->delta.text);
    break;
```

**Done Event:**
```c
case IK_STREAM_DONE:
    // Finalize accumulated content
    ik_repl_finalize_message(repl, event->done.finish_reason);
    // Save to database with metadata
    ik_repl_save_message(repl);
    break;
```

**Error Event:**
```c
case IK_STREAM_ERROR:
    // Display error in scrollback
    ik_scrollback_append_error(repl->scrollback, event->error.message);
    // Abort pending request
    ik_repl_abort_request(repl);
    break;
```

### Message Persistence

Store provider metadata in JSONB `data` column:

| Field | Type | Description |
|-------|------|-------------|
| `provider` | string | Provider name (e.g., "anthropic") |
| `model` | string | Model ID (e.g., "claude-sonnet-4-5-20250929") |
| `thinking_level` | string | Thinking level (e.g., "med") |
| `thinking` | string | Thinking content if present |
| `thinking_tokens` | int | Thinking token count |
| `input_tokens` | int | Input token count |
| `output_tokens` | int | Output token count |
| `total_tokens` | int | Total token count |

**Example JSONB:**
```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-5-20250929",
  "thinking_level": "med",
  "thinking": "Let me think about this...",
  "thinking_tokens": 150,
  "input_tokens": 1200,
  "output_tokens": 350,
  "total_tokens": 1550
}
```

Use existing `ik_db_message_insert()` - JSONB column is flexible, no schema changes needed.

### Backward Compatibility

- Remove OpenAI-specific callback handling
- Replace `ik_openai_event_t` with `ik_stream_event_t`
- Streaming behavior identical to before from user perspective
- Message format compatible with existing data

## Test Scenarios

### Stream Callback
- Content delta: appends to scrollback
- Multiple content deltas: accumulate correctly
- Tool call delta: accumulates id, name, arguments
- Thinking delta: stored separately
- Done event: saves complete message
- Error event: displays error, aborts request

### Message Persistence
- Message saved with provider metadata
- Thinking content stored when present
- Token counts stored correctly
- Missing thinking: field omitted (not null)
- JSONB data queryable after save

### UI Updates
- Scrollback updates during streaming
- Render triggered on each delta
- Final message displayed correctly
- Error messages displayed in scrollback

## Postconditions

- [ ] Stream callback handles all `ik_stream_event_t` types
- [ ] Content deltas accumulated correctly
- [ ] Tool calls accumulated correctly
- [ ] Thinking content stored separately
- [ ] Messages saved with provider metadata
- [ ] Token counts persisted
- [ ] No OpenAI-specific event handling remains
- [ ] UI updates correctly during streaming
- [ ] Compiles without warnings
- [ ] `make check` passes
